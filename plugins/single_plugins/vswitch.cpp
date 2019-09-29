#include <plugin.hpp>
#include <output.hpp>
#include <core.hpp>
#include <debug.hpp>
#include <view.hpp>
#include <view-transform.hpp>
#include <render-manager.hpp>
#include <workspace-manager.hpp>

#include <queue>
#include <linux/input.h>
#include <utility>
#include <animation.hpp>
#include <set>
#include "view-change-viewport-signal.hpp"
#include "../wobbly/wobbly-signal.hpp"

class vswitch_view_transformer : public wf_2D_view
{
    public:
        static const std::string name;
        vswitch_view_transformer(wayfire_view view) : wf_2D_view(view) {}
        virtual uint32_t get_z_order() override { return WF_TRANSFORMER_BLUR - 1; }
};
const std::string vswitch_view_transformer::name = "vswitch-transformer";

class vswitch : public wf::plugin_interface_t
{
    private:
        activator_callback callback_left, callback_right, callback_up, callback_down;
        activator_callback callback_win_left, callback_win_right, callback_win_up, callback_win_down;
        activator_callback callback_add_to_group_1, callback_toggle_group_1;
        activator_callback callback_add_to_group_2, callback_toggle_group_2;
        activator_callback callback_add_to_group_3, callback_toggle_group_3;
        activator_callback callback_add_to_group_4, callback_toggle_group_4;
        activator_callback callback_add_to_group_5, callback_toggle_group_5;
        activator_callback callback_add_to_group_6, callback_toggle_group_6;
        activator_callback callback_add_to_group_7, callback_toggle_group_7;
        activator_callback callback_add_to_group_8, callback_toggle_group_8;
        activator_callback callback_add_to_group_9, callback_toggle_group_9;

        gesture_callback gesture_cb;

        wf_duration duration;
        wf_transition dx, dy;
        wayfire_view grabbed_view = nullptr;
        std::set<wayfire_view> one, two, three, four, five, six, seven, eight, nine;

        wf_option animation_duration;

    public:
    wayfire_view get_top_view()
    {
        auto ws = output->workspace->get_current_workspace();
        auto views = output->workspace->get_views_on_workspace(ws,
            wf::LAYER_WORKSPACE | wf::LAYER_FULLSCREEN, true);

        return views.empty() ? nullptr : views[0];
    }

    void init(wayfire_config *config)
    {
        grab_interface->name = "vswitch";
        grab_interface->capabilities = wf::CAPABILITY_MANAGE_DESKTOP;
        grab_interface->callbacks.cancel = [=] () {stop_switch();};

        callback_left  = [=] (wf_activator_source, uint32_t) { add_direction(-1,  0); };
        callback_right = [=] (wf_activator_source, uint32_t) { add_direction( 1,  0); };
        callback_up    = [=] (wf_activator_source, uint32_t) { add_direction( 0, -1); };
        callback_down  = [=] (wf_activator_source, uint32_t) { add_direction( 0,  1); };

        callback_win_left  = [=] (wf_activator_source, uint32_t) { add_direction(-1,  0, get_top_view()); };
        callback_win_right = [=] (wf_activator_source, uint32_t) { add_direction( 1,  0, get_top_view()); };
        callback_win_up    = [=] (wf_activator_source, uint32_t) { add_direction( 0, -1, get_top_view()); };
        callback_win_down  = [=] (wf_activator_source, uint32_t) { add_direction( 0,  1, get_top_view()); };

        callback_add_to_group_1  = [=] (wf_activator_source, uint32_t) { add_to_group(1, 0, &one); };
        callback_add_to_group_2  = [=] (wf_activator_source, uint32_t) { add_to_group(2, 0, &two); };
        callback_add_to_group_3  = [=] (wf_activator_source, uint32_t) { add_to_group(3, 0, &three); };
        callback_add_to_group_4  = [=] (wf_activator_source, uint32_t) { add_to_group(4, 0, &four); };
        callback_add_to_group_5  = [=] (wf_activator_source, uint32_t) { add_to_group(5, 0, &five); };
        callback_add_to_group_6  = [=] (wf_activator_source, uint32_t) { add_to_group(6, 0, &six); };
        callback_add_to_group_7  = [=] (wf_activator_source, uint32_t) { add_to_group(7, 0, &seven); };
        callback_add_to_group_8  = [=] (wf_activator_source, uint32_t) { add_to_group(8, 0, &eight); };
        callback_add_to_group_9  = [=] (wf_activator_source, uint32_t) { add_to_group(9, 0, &nine); };

        callback_toggle_group_1 = [=] (wf_activator_source, uint32_t) { toggle_group(1, 0, &one); };
        callback_toggle_group_2 = [=] (wf_activator_source, uint32_t) { toggle_group(2, 0, &two); };
        callback_toggle_group_3 = [=] (wf_activator_source, uint32_t) { toggle_group(3, 0, &three); };
        callback_toggle_group_4 = [=] (wf_activator_source, uint32_t) { toggle_group(4, 0, &four); };
        callback_toggle_group_5 = [=] (wf_activator_source, uint32_t) { toggle_group(5, 0, &five); };
        callback_toggle_group_6 = [=] (wf_activator_source, uint32_t) { toggle_group(6, 0, &six); };
        callback_toggle_group_7 = [=] (wf_activator_source, uint32_t) { toggle_group(7, 0, &seven); };
        callback_toggle_group_8 = [=] (wf_activator_source, uint32_t) { toggle_group(8, 0, &eight); };
        callback_toggle_group_9 = [=] (wf_activator_source, uint32_t) { toggle_group(9, 0, &nine); };

        auto section   = config->get_section("vswitch");

        auto binding_left  = section->get_option("binding_left",  "<super> KEY_LEFT  | swipe right 4");
        auto binding_right = section->get_option("binding_right", "<super> KEY_RIGHT | swipe left 4");
        auto binding_up    = section->get_option("binding_up",    "<super> KEY_UP    | swipe down 4");
        auto binding_down  = section->get_option("binding_down",  "<super> KEY_DOWN  | swipe up 4");

        auto binding_win_left  = section->get_option("binding_win_left",  "<super> <shift> KEY_LEFT");
        auto binding_win_right = section->get_option("binding_win_right", "<super> <shift> KEY_RIGHT");
        auto binding_win_up    = section->get_option("binding_win_up",    "<super> <shift> KEY_UP");
        auto binding_win_down  = section->get_option("binding_win_down",  "<super> <shift> KEY_DOWN");

        auto binding_add_to_group_1  = section->get_option("binding_add_to_group_1", "<alt> <shift> KEY_1");
        auto binding_toggle_group_1  = section->get_option("binding_toggle_group_1",         "<alt> KEY_1");
        auto binding_add_to_group_2  = section->get_option("binding_add_to_group_2", "<alt> <shift> KEY_2");
        auto binding_toggle_group_2  = section->get_option("binding_toggle_group_2",         "<alt> KEY_2");
        auto binding_add_to_group_3  = section->get_option("binding_add_to_group_3", "<alt> <shift> KEY_3");
        auto binding_toggle_group_3  = section->get_option("binding_toggle_group_3",         "<alt> KEY_3");
        auto binding_add_to_group_4  = section->get_option("binding_add_to_group_4", "<alt> <shift> KEY_4");
        auto binding_toggle_group_4  = section->get_option("binding_toggle_group_4",         "<alt> KEY_4");
        auto binding_add_to_group_5  = section->get_option("binding_add_to_group_5", "<alt> <shift> KEY_5");
        auto binding_toggle_group_5  = section->get_option("binding_toggle_group_5",         "<alt> KEY_5");
        auto binding_add_to_group_6  = section->get_option("binding_add_to_group_6", "<alt> <shift> KEY_6");
        auto binding_toggle_group_6  = section->get_option("binding_toggle_group_6",         "<alt> KEY_6");
        auto binding_add_to_group_7  = section->get_option("binding_add_to_group_7", "<alt> <shift> KEY_7");
        auto binding_toggle_group_7  = section->get_option("binding_toggle_group_7",         "<alt> KEY_7");
        auto binding_add_to_group_8  = section->get_option("binding_add_to_group_8", "<alt> <shift> KEY_8");
        auto binding_toggle_group_8  = section->get_option("binding_toggle_group_8",         "<alt> KEY_8");
        auto binding_add_to_group_9  = section->get_option("binding_add_to_group_9", "<alt> <shift> KEY_9");
        auto binding_toggle_group_9  = section->get_option("binding_toggle_group_9",         "<alt> KEY_9");

        output->add_activator(binding_left,  &callback_left);
        output->add_activator(binding_right, &callback_right);
        output->add_activator(binding_up,    &callback_up);
        output->add_activator(binding_down,  &callback_down);

        output->add_activator(binding_win_left,  &callback_win_left);
        output->add_activator(binding_win_right, &callback_win_right);
        output->add_activator(binding_win_up,    &callback_win_up);
        output->add_activator(binding_win_down,  &callback_win_down);

        output->add_activator(binding_add_to_group_1,  &callback_add_to_group_1);
        output->add_activator(binding_toggle_group_1,  &callback_toggle_group_1);
        output->add_activator(binding_add_to_group_2,  &callback_add_to_group_2);
        output->add_activator(binding_toggle_group_2,  &callback_toggle_group_2);
        output->add_activator(binding_add_to_group_3,  &callback_add_to_group_3);
        output->add_activator(binding_toggle_group_3,  &callback_toggle_group_3);
        output->add_activator(binding_add_to_group_4,  &callback_add_to_group_4);
        output->add_activator(binding_toggle_group_4,  &callback_toggle_group_4);
        output->add_activator(binding_add_to_group_5,  &callback_add_to_group_5);
        output->add_activator(binding_toggle_group_5,  &callback_toggle_group_5);
        output->add_activator(binding_add_to_group_6,  &callback_add_to_group_6);
        output->add_activator(binding_toggle_group_6,  &callback_toggle_group_6);
        output->add_activator(binding_add_to_group_7,  &callback_add_to_group_7);
        output->add_activator(binding_toggle_group_7,  &callback_toggle_group_7);
        output->add_activator(binding_add_to_group_8,  &callback_add_to_group_8);
        output->add_activator(binding_toggle_group_8,  &callback_toggle_group_8);
        output->add_activator(binding_add_to_group_9,  &callback_add_to_group_9);
        output->add_activator(binding_toggle_group_9,  &callback_toggle_group_9);

        animation_duration = section->get_option("duration", "180");
        duration = wf_duration(animation_duration);

        output->connect_signal("set-workspace-request", &on_set_workspace_request);
        output->connect_signal("detach-view", &on_destroy_view);
        output->connect_signal("view-disappeared", &on_destroy_view);
    }

    inline bool is_active()
    {
        return output->is_plugin_active(grab_interface->name);
    }

    void add_to_group(int x, int y, std::set<wayfire_view> *group)
    {
        group->insert(get_top_view());
        wf_point ws = output->workspace->get_current_workspace();
        ws.x = x;
        ws.y = y;
        output->workspace->move_to_workspace(get_top_view(), ws);
        output->focus_view(get_top_view());
    }

    void toggle_group(int x, int y, std::set<wayfire_view> *group)
    {
        wf_point cws = output->workspace->get_current_workspace();
        std::vector<wayfire_view> current_views = output->workspace->get_views_on_workspace(cws, 4, true);
        std::set<wayfire_view> s(current_views.begin(), current_views.end());
        bool answer = std::includes(s.begin(), s.end(), group->begin(), group->end());
        //log_error("\ncurr size: %lu X one size: %lu\n", current_views.size(), one.size());

        if (answer){
            cws.x = x;
            cws.y = y;
        }
        
        for(auto w : *group) {
            output->workspace->move_to_workspace(w, cws);
        }    
        if (answer){
            output->focus_view(get_top_view());
        }else{
            output->focus_view(*(group->begin()));
            output->workspace->bring_to_front(*(group->begin()));
        }

    };


    void add_direction(int x, int y, wayfire_view view = nullptr)
    {
        if (!x && !y)
            return;

        if (!is_active())
            start_switch();

        if (view && view->role != wf::VIEW_ROLE_TOPLEVEL)
            view = nullptr;

        if (view && !grabbed_view)
            grabbed_view = view;

        /* Make sure that when we add this direction, we won't go outside
         * of the workspace grid */
        auto cws = output->workspace->get_current_workspace();
        auto wsize = output->workspace->get_workspace_grid_size();
        int tvx = clamp(cws.x + dx.end + x, 0.0, wsize.width - 1.0);
        int tvy = clamp(cws.y + dy.end + y, 0.0, wsize.height - 1.0);

        dx = {duration.progress(dx), 1.0 * tvx - cws.x};
        dy = {duration.progress(dy), 1.0 * tvy - cws.y};

        duration.start();
    }

    wf::signal_callback_t on_set_workspace_request = [=] (wf::signal_data_t *data)
    {
        if (is_active())
            return;

        auto ev = static_cast<change_viewport_signal*> (data);
        ev->carried_out = true;
        add_direction(ev->new_viewport.x - ev->old_viewport.x,
            ev->new_viewport.y - ev->old_viewport.y);
    };

    wf::signal_callback_t on_destroy_view = [=] (wf::signal_data_t *data)
    {
        wayfire_view w = get_signaled_view(data);
        one.erase(w);
    };

    std::vector<wayfire_view> get_ws_views()
    {
        std::vector<wayfire_view> views;
        return views;
        for (auto& view : output->workspace->get_views_in_layer(wf::MIDDLE_LAYERS))
        {
            if (view == grabbed_view)
                views.push_back(view);
        }

        return views;
    }

    void ensure_transformer(wayfire_view view)
    {
        if (!view->get_transformer(vswitch_view_transformer::name))
        {
            view->add_transformer(
                std::make_unique<vswitch_view_transformer>(view),
                vswitch_view_transformer::name);
        }
    }

    bool start_switch()
    {
        if (!output->activate_plugin(grab_interface))
            return false;

        output->render->add_effect(&update_animation, wf::OUTPUT_EFFECT_PRE);
        output->render->set_redraw_always();

        duration.start();
        dx = dy = {0, 0};

        return true;
    }

    wf::effect_hook_t update_animation = [=] ()
    {
        if (!duration.running())
            return stop_switch();
    };

    void slide_done()
    {
        auto cws = output->workspace->get_current_workspace();
        auto old_ws = cws;

        cws.x += dx.end;
        cws.y += dy.end;

        auto output_g = output->get_relative_geometry();
        output->workspace->set_workspace(cws);

        if (grabbed_view)
        {
            auto wm = grabbed_view->get_wm_geometry();
            grabbed_view->move(wm.x + dx.end * output_g.width,
                wm.y + dy.end * output_g.height);

            output->focus_view(grabbed_view);
            output->workspace->bring_to_front(grabbed_view);

            view_change_viewport_signal data;
            data.view = grabbed_view;
            data.from = old_ws;
            data.to = output->workspace->get_current_workspace();
            output->emit_signal("view-change-viewport", &data);
        }
    }

    void stop_switch()
    {
        slide_done();
        grabbed_view = nullptr;

        for (auto view : get_ws_views())
            view->pop_transformer(vswitch_view_transformer::name);

        output->deactivate_plugin(grab_interface);
        output->render->rem_effect(&update_animation);
        output->render->set_redraw_always(false);
    }

    void fini()
    {
        if (!is_active())
            stop_switch();

        output->rem_binding(&callback_left);
        output->rem_binding(&callback_right);
        output->rem_binding(&callback_up);
        output->rem_binding(&callback_down);

        output->rem_binding(&callback_win_left);
        output->rem_binding(&callback_win_right);
        output->rem_binding(&callback_win_up);
        output->rem_binding(&callback_win_down);

        output->rem_binding(&gesture_cb);
        output->disconnect_signal("set-workspace-request",
            &on_set_workspace_request);
        output->disconnect_signal("view-disappeared", &on_destroy_view);
        output->disconnect_signal("detach-view", &on_destroy_view);
    }
};

DECLARE_WAYFIRE_PLUGIN(vswitch);
