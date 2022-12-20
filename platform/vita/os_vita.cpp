/*************************************************************************/
/*  os_vita.cpp                                                          */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "os_vita.h"

#include "core/array.h"
#include "drivers/dummy/rasterizer_dummy.h"
#include "drivers/dummy/texture_loader_dummy.h"
#include "drivers/gles2/rasterizer_gles2.h"
#include "drivers/unix/dir_access_unix.h"
#include "drivers/unix/file_access_unix.h"
#include "drivers/unix/ip_unix.h"
#include "drivers/unix/net_socket_posix.h"
#include "drivers/unix/thread_posix.h"
#include "main/main.h"
#include "servers/audio_server.h"
#include "servers/visual/visual_server_raster.h"
#include "servers/visual/visual_server_wrap_mt.h"

int OS_Vita::get_video_driver_count() const {
	return 1;
}

int OS_Vita::get_audio_driver_count() const {
	return 1;
}

const char *OS_Vita::get_audio_driver_name(int p_driver) const {
	return "Vita";
}

void OS_Vita::initialize_core() {
#if !defined(NO_THREADS)
	init_thread_posix();
#endif

	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_FILESYSTEM);

#ifndef NO_NETWORK
	NetSocketPosix::make_default();
	IP_Unix::make_default();
#endif
}

void OS_Vita::finalize_core() {
#ifndef NO_NETWORK
	NetSocketPosix::cleanup();
#endif
}

int OS_Vita::get_current_video_driver() const {
	return video_driver_index;
}

Error OS_Vita::initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) {
	bool gl_initialization_error = false;
	bool gles2 = false;
	gl_context = NULL;

	if (p_video_driver == VIDEO_DRIVER_GLES2) {
		gles2 = true;
	} else if (GLOBAL_GET("rendering/quality/driver/fallback_to_gles2")) {
		p_video_driver = VIDEO_DRIVER_GLES2;
		gles2 = true;
	} else {
		OS::get_singleton()->alert("OpenGL ES 3 is not supported on this device.\n\n"
								   "Please enable the option \"Fallback to OpenGL ES 2.0\" in the options menu.\n",
				"OpenGL ES 3 Not Supported");
		gl_initialization_error = true;
	}

	if (!gl_initialization_error) {
		gl_context = memnew(ContextEGL_Vita(gles2));
		if (gl_context->initialize()) {
			OS::get_singleton()->alert("Failed to initialize OpenGL ES 2.0\n"
									   "OpenGL ES 2.0 Initialization Failed");
			memdelete(gl_context);
			gl_context = NULL;
			gl_initialization_error = true;
		}
		if (RasterizerGLES2::is_viable() == OK) {
			RasterizerGLES2::register_config();
			RasterizerGLES2::make_current();
		} else {
			OS::get_singleton()->alert("RasterizerGLES2::is_viable() failed\n"
									   "RasterizerGLES2 Not Viable");
			memdelete(gl_context);
			gl_context = NULL;
			gl_initialization_error = true;
		}
	}

	if (gl_initialization_error) {
		OS::get_singleton()->alert("Your device does not support any of the supported OpenGL versions.\n"
								   "Please check your graphics drivers and try again.\n",
				"Graphics Driver Error");
		return ERR_UNAVAILABLE;
	}

	video_driver_index = p_video_driver;

	visual_server = memnew(VisualServerRaster);
	if (get_render_thread_mode() != RENDER_THREAD_UNSAFE) {
		visual_server = memnew(VisualServerWrapMT(visual_server, false));
	}

	visual_server->init();

	AudioDriverManager::initialize(p_audio_driver);

	input = memnew(InputDefault);
	input->set_use_input_buffering(true);
	input->set_emulate_mouse_from_touch(true);
	joypad = memnew(JoypadVita(input));

	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

	sceTouchGetPanelInfo(0, &front_panel_info);
	front_panel_size = Vector2(front_panel_info.maxAaX, front_panel_info.maxAaY);

	return OK;
}

void OS_Vita::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
	input->set_main_loop(p_main_loop);
}

void OS_Vita::delete_main_loop() {
	memdelete(main_loop);
}

void OS_Vita::finalize() {
	memdelete(joypad);
	memdelete(input);
	visual_server->finish();
	memdelete(visual_server);
	memdelete(gl_context);
}

void OS_Vita::alert(const String &p_alert, const String &p_title) {
	sceClibPrintf(p_alert.ascii().get_data());
}

Point2 OS_Vita::get_mouse_position() const {
	return Point2(0, 0);
}

int OS_Vita::get_mouse_button_state() const {
	return 0;
}

void OS_Vita::set_window_title(const String &p_title) {
}

void OS_Vita::set_video_mode(const VideoMode &p_video_mode, int p_screen) {
}

OS::VideoMode OS_Vita::get_video_mode(int p_screen) const {
	return video_mode;
}

void OS_Vita::get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen) const {
	p_list->push_back(video_mode);
}

Size2 OS_Vita::get_window_size() const {
	return Size2(video_mode.width, video_mode.height);
}

String OS_Vita::get_name() const {
	return "Vita";
}

MainLoop *OS_Vita::get_main_loop() const {
	return main_loop;
}

void OS_Vita::swap_buffers() {
	gl_context->swap_buffers();
}

bool OS_Vita::can_draw() const {
	return true;
}

void OS_Vita::run() {
	if (!main_loop)
		return;

	main_loop->init();

	while (true) {
		joypad->process_joypads();
		process_touch();

		if (Main::iteration())
			break;
	};

	main_loop->finish();
}

void OS_Vita::process_touch() {
	sceTouchPeek(0, &touch, 1);
	static uint32_t last_touch_count = 0;

	if (touch.reportNum != last_touch_count) {
		if (touch.reportNum > last_touch_count) { // new touches
			for (uint32_t i = last_touch_count; i < touch.reportNum; i++) {
				Vector2 pos(touch.report[i].x, touch.report[i].y);
				pos /= front_panel_size;
				pos *= Vector2(960, 544);
				Ref<InputEventScreenTouch> st;
				st.instance();
				st->set_index(i);
				st->set_position(pos);
				st->set_pressed(true);
				input->parse_input_event(st);
			}
		} else { // lost touches
			for (uint32_t i = touch.reportNum; i < last_touch_count; i++) {
				Ref<InputEventScreenTouch> st;
				st.instance();
				st->set_index(i);
				st->set_position(last_touch_pos[i]);
				st->set_pressed(false);
				input->parse_input_event(st);
			}
		}
	} else {
		for (uint32_t i = 0; i < touch.reportNum; i++) {
			Vector2 pos(touch.report[i].x, touch.report[i].y);
			pos /= front_panel_size;
			pos *= Vector2(960, 544);
			Ref<InputEventScreenDrag> sd;
			sd.instance();
			sd->set_index(i);
			sd->set_position(pos);
			sd->set_relative(pos - last_touch_pos[i]);
			last_touch_pos[i] = pos;
			input->parse_input_event(sd);
		}
	}

	last_touch_count = touch.reportNum;
}

String OS_Vita::get_data_path() const {
	return "ux0:/data";
}

String OS_Vita::get_user_data_dir() const {
	String appname = get_safe_dir_name(ProjectSettings::get_singleton()->get("application/config/name"));
	if (appname != "") {
		bool use_custom_dir = ProjectSettings::get_singleton()->get("application/config/use_custom_user_dir");
		if (use_custom_dir) {
			String custom_dir = get_safe_dir_name(ProjectSettings::get_singleton()->get("application/config/custom_user_dir_name"), true);
			if (custom_dir == "") {
				custom_dir = appname;
			}
			DirAccess *dir_access = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			dir_access->make_dir_recursive(get_data_path().plus_file(custom_dir));
			memdelete(dir_access);
			return get_data_path().plus_file(custom_dir);
		} else {
			DirAccess *dir_access = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			dir_access->make_dir_recursive(get_data_path().plus_file(get_godot_dir_name()).plus_file("app_userdata").plus_file(appname));
			memdelete(dir_access);
			return get_data_path().plus_file(get_godot_dir_name()).plus_file("app_userdata").plus_file(appname);
		}
	}
	DirAccess *dir_access = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	dir_access->make_dir_recursive(get_data_path().plus_file(get_godot_dir_name()).plus_file("app_userdata").plus_file("__unknown"));
	memdelete(dir_access);
	return get_data_path().plus_file(get_godot_dir_name()).plus_file("app_userdata").plus_file("__unknown");
}

String OS_Vita::get_model_name() const {
	return "Sony Playstation Vita";
}

bool OS_Vita::has_virtual_keyboard() const {
	return true;
}

int OS_Vita::get_virtual_keyboard_height() const {
	return 200;
}

void OS_Vita::show_virtual_keyboard(const String &p_existing_text, const Rect2 &p_screen_rect, bool p_multiline, int p_max_input_length, int p_cursor_start, int p_cursor_end) {
}

void OS_Vita::hide_virtual_keyboard() {
}

void OS_Vita::set_offscreen_gl_available(bool p_available) {
	secondary_gl_available = false;
}

bool OS_Vita::is_offscreen_gl_available() const {
	return secondary_gl_available;
}

void OS_Vita::set_offscreen_gl_current(bool p_current) {
}

bool OS_Vita::_check_internal_feature_support(const String &p_feature) {
	if (p_feature == "mobile") {
		//TODO support etc2 only if GLES3 driver is selected
		return true;
	}
	if (p_feature == "armeabi-v7a" || p_feature == "armeabi") {
		return true;
	}
	return false;
}

OS_Vita::OS_Vita() {
	video_mode.width = 960;
	video_mode.height = 544;
	video_mode.fullscreen = true;
	video_mode.resizable = false;

	video_driver_index = 0;
	main_loop = nullptr;
	visual_server = nullptr;
	gl_context = nullptr;

	AudioDriverManager::add_driver(&driver_vita);
}

OS_Vita::~OS_Vita() {
	video_driver_index = 0;
	main_loop = nullptr;
	visual_server = nullptr;
	input = nullptr;
	gl_context = nullptr;
}

// Misc

Error OS_Vita::execute(const String &p_path, const List<String> &p_arguments, bool p_blocking = true, ProcessID *r_child_id = nullptr, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr, bool p_open_console = false) {
	return FAILED;
}

Error OS_Vita::kill(const ProcessID &p_pid) {
	return FAILED;
}

bool OS_Vita::is_process_running(const ProcessID &p_pid) const {
	return false;
}

bool OS_Vita::has_environment(const String &p_var) const {
	return false;
}

String OS_Vita::get_environment(const String &p_var) const {
	return "";
}

bool OS_Vita::set_environment(const String &p_var, const String &p_value) const {
	return false;
}

OS::Date OS_Vita::get_date(bool local) const {
	return OS::Date();
}

OS::Time OS_Vita::get_time(bool local) const {
	return OS::Time();
}

OS::TimeZoneInfo OS_Vita::get_time_zone_info() const {
	return OS::TimeZoneInfo();
}

void OS_Vita::delay_usec(uint32_t p_usec) const {
	sceKernelDelayThread(p_usec);
}

uint64_t OS_Vita::get_ticks_usec() const {
	static int tick_resolution = sceRtcGetTickResolution();
	SceRtcTick current_tick;
	sceRtcGetCurrentTick(&current_tick);
	return current_tick.tick / (tick_resolution / 1000000);
}

String OS_Vita::get_stdin_string(bool p_block) {
	return "";
}
