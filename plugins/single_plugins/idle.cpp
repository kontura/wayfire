extern "C"
{
#include <wlr/types/wlr_idle.h>
}

#include "singleton-plugin.hpp"
#include "output.hpp"
#include "core.hpp"
#include "config.hpp"
#include "output-layout.hpp"

class wayfire_idle
{
    bool idle_enabled = true;
    wlr_idle_timeout *timeout = NULL;
    wf::wl_listener_wrapper on_idle, on_resume;

    wf_option dpms_timeout;
    wf_option_callback dpms_timeout_updated = [=] () {
        create_timeout(dpms_timeout->as_int());
    };

    public:
    wayfire_idle()
    {
        dpms_timeout = wf::get_core().config->get_section("idle")
            ->get_option("dpms_timeout", "-1");

        dpms_timeout->add_updated_handler(&dpms_timeout_updated);
        dpms_timeout_updated();
    }

    void destroy_timeout()
    {
        if (timeout)
        {
            on_idle.disconnect();
            on_resume.disconnect();
            wlr_idle_timeout_destroy(timeout);
        }

        timeout = NULL;
    }

    void create_timeout(int timeout_sec)
    {
        destroy_timeout();
        if (timeout_sec <= 0)
            return;

        timeout = wlr_idle_timeout_create(wf::get_core().protocols.idle,
            wf::get_core().get_current_seat(), 1000 * timeout_sec);

        on_idle.set_callback([&] (void*) {
            set_state(wf::OUTPUT_IMAGE_SOURCE_SELF, wf::OUTPUT_IMAGE_SOURCE_DPMS);
        });
        on_idle.connect(&timeout->events.idle);

        on_resume.set_callback([&] (void*) {
            set_state(wf::OUTPUT_IMAGE_SOURCE_DPMS, wf::OUTPUT_IMAGE_SOURCE_SELF);
        });
        on_resume.connect(&timeout->events.resume);
    }

    ~wayfire_idle()
    {
        destroy_timeout();

        dpms_timeout->rem_updated_handler(&dpms_timeout_updated);

        /* Make sure idle is enabled */
        if (!idle_enabled)
            toggle_idle();
    }

    /* Change all outputs with state from to state to */
    void set_state(wf::output_image_source_t from, wf::output_image_source_t to)
    {
        auto config = wf::get_core().output_layout->get_current_configuration();

        for (auto& entry : config)
        {
            if (entry.second.source == from)
                entry.second.source = to;
        }

        wf::get_core().output_layout->apply_configuration(config);
    }

    void toggle_idle()
    {
        idle_enabled ^= 1;
        wlr_idle_set_enabled(wf::get_core().protocols.idle, NULL, idle_enabled);
    }
};

class wayfire_idle_singleton : public wf::singleton_plugin_t<wayfire_idle>
{
    activator_callback toggle;
    void init(wayfire_config *config) override
    {
        singleton_plugin_t::init(config);

        auto binding = config->get_section("idle")
            ->get_option("toggle", "<super> <shift> KEY_I");
        toggle = [=] (wf_activator_source, uint32_t) {
            get_instance().toggle_idle();
        };

        output->add_activator(binding, &toggle);
    }

    void fini() override
    {
        output->rem_binding(&toggle);
        singleton_plugin_t::fini();
    }
};

DECLARE_WAYFIRE_PLUGIN(wayfire_idle_singleton);
