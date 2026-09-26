#include <obs-module.h>
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <media-io/audio-io.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("c875-follow-source", "en-US")

namespace fs = std::filesystem;

namespace {

constexpr const char *kDirectory = "capture_directory";
constexpr const char *kLatency = "target_latency_ms";
constexpr const char *kHardwareDecode = "hw_decode";

struct FollowSource {
	obs_source_t *source = nullptr;
	obs_source_t *media = nullptr;
	std::string file;
	int64_t target_latency_ms = 5000;
	bool initial_seek_done = false;
	DWORD recentral_pid = 0;
	HANDLE injector_process = nullptr;
	bool injector_elevated = false;
	bool injection_finished = false;
	float injector_scan_elapsed = 2.0f;
};

const char *source_name(void *)
{
	return obs_module_text("C875FollowSource");
}

std::wstring utf8_to_wide(const char *text)
{
	if (!text || !*text)
		return {};
	int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
	std::wstring result(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), size);
	result.pop_back();
	return result;
}

std::string wide_to_utf8(const std::wstring &text)
{
	if (text.empty())
		return {};
	int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string result(static_cast<size_t>(size), '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), size, nullptr, nullptr);
	result.pop_back();
	return result;
}

DWORD find_recentral_process()
{
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return 0;
	DWORD pid = 0;
	PROCESSENTRY32W entry{};
	entry.dwSize = sizeof(entry);
	if (Process32FirstW(snapshot, &entry)) {
		do {
			if (_wcsicmp(entry.szExeFile, L"RECentral.exe") == 0 ||
			    _wcsicmp(entry.szExeFile, L"AVerRECentral.exe") == 0) {
				pid = entry.th32ProcessID;
				break;
			}
		} while (Process32NextW(snapshot, &entry));
	}
	CloseHandle(snapshot);
	return pid;
}

bool start_injector(FollowSource *context, bool elevated)
{
	char *injector_utf8 = obs_module_file("recentral_share_injector.exe");
	char *hook_utf8 = obs_module_file("recentral_share_hook.dll");
	if (!injector_utf8 || !hook_utf8) {
		blog(LOG_ERROR, "[c875-follow] Injector files are not installed");
		bfree(injector_utf8);
		bfree(hook_utf8);
		return false;
	}
	const std::wstring injector = utf8_to_wide(injector_utf8);
	const std::wstring hook = utf8_to_wide(hook_utf8);
	bfree(injector_utf8);
	bfree(hook_utf8);
	if (GetFileAttributesW(injector.c_str()) == INVALID_FILE_ATTRIBUTES ||
	    GetFileAttributesW(hook.c_str()) == INVALID_FILE_ATTRIBUTES) {
		blog(LOG_ERROR, "[c875-follow] Injector executable or hook DLL is missing");
		return false;
	}

	std::wstring parameters = L"\"" + hook + L"\"";
	SHELLEXECUTEINFOW execute{};
	execute.cbSize = sizeof(execute);
	execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
	execute.lpVerb = elevated ? L"runas" : L"open";
	execute.lpFile = injector.c_str();
	execute.lpParameters = parameters.c_str();
	execute.nShow = SW_HIDE;
	if (!ShellExecuteExW(&execute) || !execute.hProcess) {
		blog(LOG_WARNING, "[c875-follow] Could not start injector (elevated=%d, error=%lu)", elevated,
		     GetLastError());
		return false;
	}
	context->injector_process = execute.hProcess;
	context->injector_elevated = elevated;
	blog(LOG_INFO, "[c875-follow] Started automatic sharing unlock for RECentral PID %lu (elevated=%d)",
	     context->recentral_pid, elevated);
	return true;
}

void update_injector(FollowSource *context, float seconds)
{
	context->injector_scan_elapsed += seconds;
	if (context->injector_scan_elapsed < 2.0f)
		return;
	context->injector_scan_elapsed = 0.0f;

	const DWORD pid = find_recentral_process();
	if (pid != context->recentral_pid) {
		if (context->injector_process) {
			CloseHandle(context->injector_process);
			context->injector_process = nullptr;
		}
		context->recentral_pid = pid;
		context->injector_elevated = false;
		context->injection_finished = false;
		if (pid)
			start_injector(context, false);
		return;
	}
	if (!pid || context->injection_finished || !context->injector_process)
		return;

	DWORD exit_code = STILL_ACTIVE;
	if (!GetExitCodeProcess(context->injector_process, &exit_code) || exit_code == STILL_ACTIVE)
		return;
	CloseHandle(context->injector_process);
	context->injector_process = nullptr;
	if (exit_code == 0) {
		context->injection_finished = true;
		blog(LOG_INFO, "[c875-follow] RECentral recording sharing was unlocked automatically");
	} else if (!context->injector_elevated) {
		blog(LOG_INFO, "[c875-follow] Normal injection failed (exit=%lu); requesting administrator access",
		     exit_code);
		if (!start_injector(context, true))
			context->injection_finished = true;
	} else {
		context->injection_finished = true;
		blog(LOG_ERROR, "[c875-follow] Automatic sharing unlock failed (exit=%lu)", exit_code);
	}
}

std::string newest_ts(const char *directory)
{
	if (!directory || !*directory)
		return {};

	std::error_code ec;
	fs::path newest;
	fs::file_time_type newest_time{};
	bool found = false;

	for (const auto &entry : fs::directory_iterator(fs::path(utf8_to_wide(directory)), ec)) {
		if (ec)
			break;
		if (!entry.is_regular_file(ec) || entry.path().extension() != L".ts")
			continue;

		auto time = entry.last_write_time(ec);
		if (!ec && (!found || time > newest_time)) {
			found = true;
			newest = entry.path();
			newest_time = time;
		}
	}

	return found ? wide_to_utf8(newest.native()) : std::string{};
}

void release_media(FollowSource *context)
{
	if (!context->media)
		return;
	obs_source_remove_active_child(context->source, context->media);
	obs_source_release(context->media);
	context->media = nullptr;
}

void update(void *data, obs_data_t *settings)
{
	auto *context = static_cast<FollowSource *>(data);
	const char *directory = obs_data_get_string(settings, kDirectory);
	context->target_latency_ms = obs_data_get_int(settings, kLatency);
	context->file = newest_ts(directory);
	context->initial_seek_done = false;

	release_media(context);
	if (context->file.empty()) {
		blog(LOG_WARNING, "[c875-follow] No TS recording found in '%s'", directory);
		return;
	}

	obs_data_t *media_settings = obs_data_create();
	obs_data_set_string(media_settings, "local_file", context->file.c_str());
	obs_data_set_bool(media_settings, "is_local_file", true);
	obs_data_set_bool(media_settings, "looping", false);
	obs_data_set_bool(media_settings, "restart_on_activate", false);
	obs_data_set_bool(media_settings, "close_when_inactive", false);
	obs_data_set_bool(media_settings, "hw_decode", obs_data_get_bool(settings, kHardwareDecode));

	context->media = obs_source_create_private("ffmpeg_source", "C875 Follow Media", media_settings);
	obs_data_release(media_settings);
	if (!context->media) {
		blog(LOG_ERROR, "[c875-follow] Failed to create ffmpeg_source for '%s'", context->file.c_str());
		return;
	}

	obs_source_add_active_child(context->source, context->media);
	blog(LOG_INFO, "[c875-follow] Following '%s' with target latency %lld ms", context->file.c_str(),
	     static_cast<long long>(context->target_latency_ms));
}

void *create(obs_data_t *settings, obs_source_t *source)
{
	auto *context = new FollowSource;
	context->source = source;
	update(context, settings);
	return context;
}

void destroy(void *data)
{
	auto *context = static_cast<FollowSource *>(data);
	if (context->injector_process)
		CloseHandle(context->injector_process);
	release_media(context);
	delete context;
}

void video_tick(void *data, float seconds)
{
	auto *context = static_cast<FollowSource *>(data);
	update_injector(context, seconds);
	if (!context->media || context->initial_seek_done)
		return;

	const int64_t duration = obs_source_media_get_duration(context->media);
	if (duration <= context->target_latency_ms)
		return;

	const int64_t position = duration - context->target_latency_ms;
	obs_source_media_set_time(context->media, position);
	context->initial_seek_done = true;
	blog(LOG_INFO, "[c875-follow] Initial seek: duration=%lld ms position=%lld ms",
	     static_cast<long long>(duration), static_cast<long long>(position));
}

void video_render(void *data, gs_effect_t *)
{
	auto *context = static_cast<FollowSource *>(data);
	if (context->media)
		obs_source_video_render(context->media);
}

bool audio_render(void *data, uint64_t *timestamp_out, obs_source_audio_mix *audio_output, uint32_t mixers,
		  size_t channels, size_t)
{
	auto *context = static_cast<FollowSource *>(data);
	if (!context->media || obs_source_audio_pending(context->media))
		return false;

	const uint64_t timestamp = obs_source_get_audio_timestamp(context->media);
	if (!timestamp)
		return false;

	obs_source_audio_mix child_audio{};
	obs_source_get_audio_mix(context->media, &child_audio);
	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; ++mix) {
		if ((mixers & (1u << mix)) == 0)
			continue;
		for (size_t channel = 0; channel < channels; ++channel) {
			std::memcpy(audio_output->output[mix].data[channel], child_audio.output[mix].data[channel],
				    sizeof(float) * AUDIO_OUTPUT_FRAMES);
		}
	}

	*timestamp_out = timestamp;
	return true;
}

uint32_t width(void *data)
{
	auto *context = static_cast<FollowSource *>(data);
	return context->media ? obs_source_get_width(context->media) : 0;
}

uint32_t height(void *data)
{
	auto *context = static_cast<FollowSource *>(data);
	return context->media ? obs_source_get_height(context->media) : 0;
}

void defaults(obs_data_t *settings)
{
	std::string directory;
	if (const char *profile = std::getenv("USERPROFILE"))
		directory = std::string(profile) + "\\Videos\\Captures";
	obs_data_set_default_string(settings, kDirectory, directory.c_str());
	obs_data_set_default_int(settings, kLatency, 5000);
	obs_data_set_default_bool(settings, kHardwareDecode, true);
}

obs_properties_t *properties(void *)
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_path(props, kDirectory, obs_module_text("CaptureDirectory"), OBS_PATH_DIRECTORY, nullptr,
				nullptr);
	obs_properties_add_int_slider(props, kLatency, obs_module_text("TargetLatency"), 2000, 15000, 500);
	obs_properties_add_bool(props, kHardwareDecode, obs_module_text("HardwareDecode"));
	return props;
}

obs_source_info source_info = {
	.id = "c875_follow_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_COMPOSITE,
	.get_name = source_name,
	.create = create,
	.destroy = destroy,
	.get_width = width,
	.get_height = height,
	.get_defaults = defaults,
	.get_properties = properties,
	.update = update,
	.video_tick = video_tick,
	.video_render = video_render,
	.audio_render = audio_render,
};

} // namespace

bool obs_module_load(void)
{
	obs_register_source(&source_info);
	blog(LOG_INFO, "[c875-follow] Plugin loaded");
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[c875-follow] Plugin unloaded");
}
