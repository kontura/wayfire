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

static bool begins_with(std::string word, std::string prefix)
{
    if (word.length() < prefix.length())
        return false;

    return word.substr(0, prefix.length()) == prefix;
}

class groups : public wf::plugin_interface_t
{
    private:
        wf_point convert_workspace_index_to_coords(int index)
        {
            index--; //compensate for indexing from 0
            auto wsize = output->workspace->get_workspace_grid_size();
            int x = index % wsize.width;
            int y = index / wsize.width;
            return wf_point{x, y};
        }

        std::vector<activator_callback> add_to_group_cbs; 
        std::vector<activator_callback> toggle_group_cbs; 
        std::vector<std::set<wayfire_view>*> groups; 

    public:
        wayfire_view get_top_view()
        {
            auto ws = output->workspace->get_current_workspace();
            auto views = output->workspace->get_views_on_workspace(ws,
                    wf::LAYER_WORKSPACE | wf::LAYER_FULLSCREEN, true);

            return views.empty() ? nullptr : views[0];
        }

        void setup_workspace_bindings_from_config(wayfire_config_section *section)
        { 

            std::vector<std::string> workspace_numbers;
            const std::string select_prefix = "group_";
            for (auto binding : section->options)
            {
                if (begins_with(binding->name, select_prefix))
                {
                    workspace_numbers.push_back(
                            binding->name.substr(select_prefix.length()));
                }
            }

            auto base_modifier = section->get_option("base_modifier", "<super>");
            auto add_modifier = section->get_option("add_modifier", "<shift>");
            toggle_group_cbs.resize(workspace_numbers.size());
            add_to_group_cbs.resize(workspace_numbers.size());
            for (size_t i = 0; i < workspace_numbers.size(); i++)
            {
                auto binding = select_prefix + workspace_numbers[i];
                auto key = section->get_option(binding, "");
                int workspace_index = atoi(workspace_numbers[i].c_str());

                auto wsize = output->workspace->get_workspace_grid_size();
                if (workspace_index > (wsize.width * wsize.height) || workspace_index < 1){
                    continue;
                }

                wf_point target = convert_workspace_index_to_coords(workspace_index);

                //I need to create it this is just local
                std::set<wayfire_view> *group_set = new std::set<wayfire_view>; 
                groups.push_back(group_set);
                
                add_to_group_cbs[i] = [=] (wf_activator_source, uint32_t) { return add_to_group(target, group_set); };
                toggle_group_cbs[i] = [=] (wf_activator_source, uint32_t) { return toggle_group(target, group_set); };

                auto binding_add_to_group = new_static_option(base_modifier->as_string() + " " + add_modifier->as_string() + " " + key->as_string());
                auto binding_toggle_group = new_static_option(base_modifier->as_string() + " " + key->as_string());

                output->add_activator(binding_add_to_group, &(add_to_group_cbs[i]));
                output->add_activator(binding_toggle_group, &(toggle_group_cbs[i]));
            }
        }

        void init(wayfire_config *config)
        {
            grab_interface->name = "groups";
            grab_interface->capabilities = wf::CAPABILITY_MANAGE_DESKTOP;

            auto section = config->get_section("groups");
            setup_workspace_bindings_from_config(section); 

            output->connect_signal("detach-view", &on_destroy_view);
            output->connect_signal("view-disappeared", &on_destroy_view);
        }

        bool add_to_group(wf_point p, std::set<wayfire_view> *group)
        {
            wayfire_view w = get_top_view();
            if (!w)
                return false;

            if (group->find(w) != group->end()){
                group->erase(w);
            } else {
                group->insert(w);
                wf_point ws = output->workspace->get_current_workspace();
                ws.x = p.x;
                ws.y = p.y;
                output->workspace->move_to_workspace(w, ws);
                output->focus_view(get_top_view());
            }
            return true;
        }

        bool toggle_group(wf_point p, std::set<wayfire_view> *group)
        {
            wf_point cws = output->workspace->get_current_workspace();
            std::vector<wayfire_view> current_views = output->workspace->get_views_on_workspace(cws, 4, true);
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
