#include <wayfire/plugin.hpp>
#include <wayfire/output.hpp>
#include <wayfire/core.hpp>
#include <wayfire/debug.hpp>
#include <wayfire/view.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/render-manager.hpp>
#include <wayfire/workspace-manager.hpp>


#include <queue>
#include <linux/input.h>
#include <utility>
#include <set>
#include <wayfire/plugins/common/view-change-viewport-signal.hpp>
#include "../wobbly/wobbly-signal.hpp"

static bool begins_with(std::string word, std::string prefix)
{
    if (word.length() < prefix.length())
        return false;

    return word.substr(0, prefix.length()) == prefix;
}

class groups : public wf::plugin_interface_t
{
    private:
        wf::point_t convert_workspace_index_to_coords(int index)
        {
            index--; //compensate for indexing from 0
            auto wsize = output->workspace->get_workspace_grid_size();
            int x = index % wsize.width;
            int y = index / wsize.width;
            return wf::point_t{x, y};
        }

        std::vector<wf::activator_callback> add_to_group_cbs; 
        std::vector<wf::activator_callback> toggle_group_cbs; 
        std::vector<std::set<wayfire_view>*> groups; 

    public:
        wayfire_view get_top_view()
        {
            auto ws = output->workspace->get_current_workspace();
            auto views = output->workspace->get_views_on_workspace(ws,
                    wf::LAYER_WORKSPACE, true);

            return views.empty() ? nullptr : views[0];
        }

        void setup_workspace_bindings_from_config()
        { 
            auto section = wf::get_core().config.get_section("groups");

            std::vector<std::string> workspace_numbers;
            const std::string select_prefix = "group_";
            for (auto binding : section->get_registered_options())
            {
                if (begins_with(binding->get_name(), select_prefix))
                {
                    workspace_numbers.push_back(
                            binding->get_name().substr(select_prefix.length()));
                }
            }

            wf::option_wrapper_t<std::string> base_modifier{"groups/base_modifier"};
            wf::option_wrapper_t<std::string> add_modifier{"groups/add_modifier"};
            toggle_group_cbs.resize(workspace_numbers.size());
            add_to_group_cbs.resize(workspace_numbers.size());
            for (size_t i = 0; i < workspace_numbers.size(); i++)
            {
                auto binding = select_prefix + workspace_numbers[i];
                auto key = section->get_option(binding);

                int workspace_index = atoi(workspace_numbers[i].c_str());

                auto wsize = output->workspace->get_workspace_grid_size();
                if (workspace_index > (wsize.width * wsize.height) || workspace_index < 1){
                    continue;
                }

                wf::point_t target = convert_workspace_index_to_coords(workspace_index);

                //I need to create it this is just local
                std::set<wayfire_view> *group_set = new std::set<wayfire_view>; 
                groups.push_back(group_set);
                
                add_to_group_cbs[i] = [=] (wf::activator_source_t, uint32_t) { return add_to_group(target, group_set); };
                toggle_group_cbs[i] = [=] (wf::activator_source_t, uint32_t) { return toggle_group(target, group_set); };

                auto add_value = wf::option_type::from_string<wf::activatorbinding_t> (std::string(base_modifier) + " " + std::string(add_modifier) + " " + key->get_value_str());
                auto toggle_value = wf::option_type::from_string<wf::activatorbinding_t> (std::string(base_modifier) + " " + key->get_value_str());
                                        
                auto binding_add_to_group = wf::create_option(add_value.value());
                auto binding_toggle_group = wf::create_option(toggle_value.value());

                output->add_activator(binding_add_to_group, &(add_to_group_cbs[i]));
                output->add_activator(binding_toggle_group, &(toggle_group_cbs[i]));
            }
        }

        void init() override
        {
            grab_interface->name = "groups";
            grab_interface->capabilities = wf::CAPABILITY_MANAGE_DESKTOP;

            setup_workspace_bindings_from_config(); 

            output->connect_signal("detach-view", &on_destroy_view);
            output->connect_signal("view-disappeared", &on_destroy_view);
        }

        bool add_to_group(wf::point_t p, std::set<wayfire_view> *group)
        {
            wayfire_view w = get_top_view();
            if (!w)
                return false;

            if (group->find(w) != group->end()){
                group->erase(w);
            } else {
                group->insert(w);
                wf::point_t ws = output->workspace->get_current_workspace();
                ws.x = p.x;
                ws.y = p.y;
                output->workspace->move_to_workspace(w, ws);
                output->focus_view(get_top_view());
            }
            return true;
        }

        bool toggle_group(wf::point_t p, std::set<wayfire_view> *group)
        {
            wf::point_t cws = output->workspace->get_current_workspace();
            std::vector<wayfire_view> current_views = output->workspace->get_views_on_workspace(cws, (wf::LAYER_WORKSPACE), false);
            std::set<wayfire_view> s(current_views.begin(), current_views.end());
            bool answer = std::includes(s.begin(), s.end(), group->begin(), group->end());

            if (answer){
                cws.x = p.x;
                cws.y = p.y;
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
            return true;

        };

        wf::signal_callback_t on_destroy_view = [=] (wf::signal_data_t *data)
        {
            wayfire_view w = get_signaled_view(data);
            for(auto g : groups) {
                g->erase(w);
            }
        };

        void fini()
        {
            for (size_t i = 0; i < add_to_group_cbs.size(); i++)
            {
                output->rem_binding(&add_to_group_cbs[i]);
                output->rem_binding(&toggle_group_cbs[i]);
            }

            output->disconnect_signal("view-disappeared", &on_destroy_view);
            output->disconnect_signal("detach-view", &on_destroy_view);
        }
};

DECLARE_WAYFIRE_PLUGIN(groups);
