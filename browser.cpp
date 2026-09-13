#include <gtkmm.h>
#include <webkit2/webkit2.h>
#include <iostream>
#include <string>
#include <cstdlib>

class BrowserWindow : public Gtk::Window {
public:
    BrowserWindow()
        : m_root(Gtk::ORIENTATION_VERTICAL),
          m_topbar(Gtk::ORIENTATION_HORIZONTAL),
          m_statusbar(Gtk::ORIENTATION_HORIZONTAL) {

        set_title("NOVA Browser");
        set_default_size(1280, 760);
        set_position(Gtk::WindowPosition::WIN_POS_CENTER);
        set_resizable(true);
        set_name("nova_browser_window");
        set_border_width(12);

        auto css = Gtk::CssProvider::create();
        css->load_from_data(
            "window#nova_browser_window { background: #0c1220; color: white; }"
            "#browser_root { background: #101a2f; border-radius: 16px; box-shadow: 0 0 20px rgba(0,0,0,0.8); }"
            "#topbar { background: #16213c; color: white; border-radius: 12px; padding: 4px; }"
            "button { background: linear-gradient(135deg, #22395c, #11233b); color: white; border-radius: 10px; padding: 8px 16px; border: none; font-weight: bold; }"
            "button:hover { background: linear-gradient(135deg, #4aa8ff, #1c6ed9); }"
            "entry { background: #101a2d; color: white; border-radius: 10px; padding: 10px; font-size: 14px; }"
            "label { color: white; }"
            "#webview_host { background: white; border-radius: 8px; }"
            "#statusbar { background: #071120; color: #b6caff; font-size: 11px; border-radius: 10px; padding: 4px; }"
        );

        Gtk::StyleContext::add_provider_for_screen(
            Gdk::Screen::get_default(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        m_root.set_name("browser_root");
        m_root.set_spacing(8);

        m_topbar.set_name("topbar");
        m_topbar.set_spacing(8);
        m_topbar.set_margin_top(4);
        m_topbar.set_margin_bottom(4);
        m_topbar.set_margin_left(4);
        m_topbar.set_margin_right(4);

        m_home_button.set_label("Home");
        m_back_button.set_label("←");
        m_forward_button.set_label("→");
        m_refresh_button.set_label("↻");
        m_go_button.set_label("GO");

        m_home_button.signal_clicked().connect(sigc::mem_fun(*this, &BrowserWindow::on_home_clicked));
        m_back_button.signal_clicked().connect(sigc::mem_fun(*this, &BrowserWindow::on_back_clicked));
        m_forward_button.signal_clicked().connect(sigc::mem_fun(*this, &BrowserWindow::on_forward_clicked));
        m_refresh_button.signal_clicked().connect(sigc::mem_fun(*this, &BrowserWindow::on_refresh_clicked));
        m_go_button.signal_clicked().connect(sigc::mem_fun(*this, &BrowserWindow::on_go_clicked));

        m_topbar.pack_start(m_home_button, Gtk::PACK_SHRINK);
        m_topbar.pack_start(m_back_button, Gtk::PACK_SHRINK);
        m_topbar.pack_start(m_forward_button, Gtk::PACK_SHRINK);
        m_topbar.pack_start(m_refresh_button, Gtk::PACK_SHRINK);
        m_topbar.pack_start(m_address_entry, Gtk::PACK_EXPAND_WIDGET);
        m_topbar.pack_start(m_go_button, Gtk::PACK_SHRINK);

        m_address_entry.set_placeholder_text("Search or enter website");
        m_address_entry.set_text("https://example.com");
        m_address_entry.signal_activate().connect(sigc::mem_fun(*this, &BrowserWindow::on_go_clicked));

        m_root.pack_start(m_topbar, Gtk::PACK_SHRINK);

        m_webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
        auto* raw = GTK_WIDGET(m_webview);
        auto* wrapped = Glib::wrap(raw);
        Gtk::Widget* widget = wrapped;

        m_web_scroll.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
        m_web_scroll.set_name("webview_host");
        m_web_scroll.add(*widget);
        widget->show();

        m_root.pack_start(m_web_scroll, Gtk::PACK_EXPAND_WIDGET);

        m_statusbar.set_name("statusbar");
        m_statusbar.set_spacing(10);
        m_statusbar.set_margin_left(6);
        m_statusbar.set_margin_right(6);
        m_statusbar.set_margin_bottom(4);

        m_status_icon.set_text("●");
        m_status_icon.set_xalign(0.5);
        m_status_text.set_text("ready");
        m_status_url.set_text("https://example.com");

        m_statusbar.pack_start(m_status_icon, Gtk::PACK_SHRINK);
        m_statusbar.pack_start(m_status_text, Gtk::PACK_SHRINK);
        m_statusbar.pack_start(m_status_url, Gtk::PACK_EXPAND_WIDGET);

        m_root.pack_start(m_statusbar, Gtk::PACK_SHRINK);

        add(m_root);
        show_all_children();

        load_url(m_address_entry.get_text());

        g_signal_connect(m_webview, "load-changed", G_CALLBACK(load_changed_callback), this);
    }

private:
    void on_go_clicked() {
        load_url(m_address_entry.get_text());
    }

    void on_refresh_clicked() {
        webkit_web_view_reload(m_webview);
        m_status_text.set_text("refreshing");
    }

    void on_home_clicked() {
        load_url("https://example.com");
    }

    void on_back_clicked() {
        if (webkit_web_view_can_go_back(m_webview)) {
            webkit_web_view_go_back(m_webview);
        }
    }

    void on_forward_clicked() {
        if (webkit_web_view_can_go_forward(m_webview)) {
            webkit_web_view_go_forward(m_webview);
        }
    }

    static void load_changed_callback(WebKitWebView* view, WebKitLoadEvent event, BrowserWindow* self) {
        if (event == WEBKIT_LOAD_FINISHED) {
            const char* final_url = webkit_web_view_get_uri(view);
            if (final_url) self->m_status_url.set_text(final_url);
            self->m_status_text.set_text("loaded");
        }
    }

    void load_url(const std::string& raw) {
        std::string url = raw;
        if (url.empty()) url = "https://example.com";
        if (url.find("://") == std::string::npos) {
            url = "https://" + url;
        }
        m_address_entry.set_text(url);
        webkit_web_view_load_uri(m_webview, url.c_str());
        m_status_text.set_text("loading");
        m_status_url.set_text(url);
    }

    Gtk::Box m_root;
    Gtk::Box m_topbar;
    Gtk::Box m_statusbar;
    Gtk::Entry m_address_entry;
    Gtk::Button m_home_button;
    Gtk::Button m_back_button;
    Gtk::Button m_forward_button;
    Gtk::Button m_refresh_button;
    Gtk::Button m_go_button;
    Gtk::Label m_status_icon;
    Gtk::Label m_status_text;
    Gtk::Label m_status_url;
    Gtk::ScrolledWindow m_web_scroll;
    WebKitWebView* m_webview;
};

int main(int argc, char** argv) {
    auto app = Gtk::Application::create(argc, argv, "com.nova.browser");
    BrowserWindow win;
    return app->run(win);
}
