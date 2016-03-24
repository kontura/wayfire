#include "core.hpp"
#include "output.hpp"
#include <stdio.h>

bool keyboard_key(wlc_handle view, uint32_t time,
        const struct wlc_modifiers *modifiers, uint32_t key,
        enum wlc_key_state state) {
    uint32_t sym = wlc_keyboard_get_keysym_for_key(key, NULL);

    Output *output = core->get_active_output();
    bool grabbed = output->input->process_key_event(sym, modifiers->mods, state);

    if (output->should_redraw())
        wlc_output_schedule_render(output->get_handle());

    return grabbed;
}

bool pointer_button(wlc_handle view, uint32_t time,
        const struct wlc_modifiers *modifiers, uint32_t button,
        enum wlc_button_state state, const struct wlc_point *position) {

    assert(core);

    Output *output = core->get_active_output();
    bool grabbed = output->input->process_button_event(button, modifiers->mods, state, *position);
    if (output->should_redraw())
        wlc_output_schedule_render(output->get_handle());

    return grabbed;
}

bool pointer_motion(wlc_handle view, uint32_t time, const struct wlc_point *position) {
    assert(core);
    wlc_pointer_set_position(position);

    auto output = core->get_active_output();
    bool grabbed = output->input->process_pointer_motion_event(*position);

    if (output->should_redraw())
        wlc_output_schedule_render(output->get_handle());

    return grabbed;
}

static void
cb_log(enum wlc_log_type type, const char *str) {
    (void)type;
    printf("%s\n", str);
}

bool view_created(wlc_handle view) {
    assert(core);
    core->add_view(view);
    return true;
}

void view_destroyed(wlc_handle view) {
    assert(core);

    core->rem_view(view);
    core->focus_view(core->get_active_output()->get_active_view());
}

void view_focus(wlc_handle view, bool focus) {
    wlc_view_set_state(view, WLC_BIT_ACTIVATED, focus);
    wlc_view_bring_to_front(view);
}

void view_request_resize(wlc_handle view, uint32_t edges, const struct wlc_point *origin) {
    SignalListenerData data;

    auto v = core->find_view(view);
    if (!v)
       return;

    data.push_back((void*)(&v));
    data.push_back((void*)(origin));

    v->output->signal->trigger_signal("resize-request", data);
}


void view_request_move(wlc_handle view, const struct wlc_point *origin) {
    SignalListenerData data;

    auto v = core->find_view(view);
    if (!v)
       return;

    data.push_back((void*)(&v));
    data.push_back((void*)(origin));

    v->output->signal->trigger_signal("move-request", data);
}

void output_pre_paint(wlc_handle output) {
    assert(core);

    Output *o;
    if (!(o = core->get_output(output))) {
        core->add_output(output);
        o = core->get_output(output);
    }
    o->render->paint();
}

void output_post_paint(wlc_handle output) {
    assert(core);

    auto o = core->get_output(output);
    if (!o) return;

    o->hook->run_hooks();
    if (o->should_redraw()) {
        wlc_output_schedule_render(output);
        o->for_each_view([] (View v) {
            wlc_surface_flush_frame_callbacks(v->get_surface());
        });
    }
}

bool output_created(wlc_handle output) {
    //core->add_output(output);
    return true;
}

void log(wlc_log_type type, const char *msg) {
    std::cout << "wlc: " << msg << std::endl;
}

void view_request_geometry(wlc_handle view, const wlc_geometry *g) {
    auto v = core->find_view(view);
    if (!v) return;

    /* TODO: add pending changes for views that are not visible */
    if(v->is_visible() || v->default_mask == 0) {
        v->set_geometry(g->origin.x, g->origin.y, g->size.w, g->size.h);
        v->set_mask(v->output->viewport->get_mask_for_view(v));
    }
}

void view_request_state(wlc_handle view, wlc_view_state_bit state, bool toggle) {
    if(state == WLC_BIT_MAXIMIZED || state == WLC_BIT_FULLSCREEN)
        wlc_view_set_state(view, state, false);
    else
        wlc_view_set_state(view, state, toggle);
}

void output_focus(wlc_handle output, bool is_focused) {
//    if (is_focused)
//        core->focus_output(core->get_output(output));
}

void view_move_to_output(wlc_handle view, wlc_handle old, wlc_handle new_output) {
    core->move_view_to_output(core->find_view(view), core->get_output(old), core->get_output(new_output));
}

int main(int argc, char *argv[]) {
    static struct wlc_interface interface;
    wlc_log_set_handler(log);

    interface.view.created        = view_created;
    interface.view.destroyed      = view_destroyed;
    interface.view.focus          = view_focus;
    interface.view.move_to_output = view_move_to_output;

    interface.view.request.resize = view_request_resize;
    interface.view.request.move   = view_request_move;
    interface.view.request.geometry = view_request_geometry;
    interface.view.request.state  = view_request_state;

    interface.output.created = output_created;
    interface.output.focus = output_focus;
    interface.output.render.pre = output_pre_paint;
    interface.output.render.post = output_post_paint;

    interface.keyboard.key = keyboard_key;

    interface.pointer.button = pointer_button;
    interface.pointer.motion = pointer_motion;

    core = new Core();
    core->init();

    if (!wlc_init(&interface, argc, argv))
        return EXIT_FAILURE;

    wlc_run();
    return EXIT_SUCCESS;
}
