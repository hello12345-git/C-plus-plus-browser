#include <gtkmm.h>
#include <webkit2/webkit2.h>
#include <jsc/jsc.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <cstdio>

class BrowserWindow : public Gtk::Window {
public:
    BrowserWindow()
        : m_root(Gtk::ORIENTATION_VERTICAL),
          m_topbar(Gtk::ORIENTATION_HORIZONTAL),
          m_statusbar(Gtk::ORIENTATION_HORIZONTAL),
          m_search_box(Gtk::ORIENTATION_HORIZONTAL) {

        set_title("NOVA Browser");
        set_default_size(1280, 760);
        set_position(Gtk::WindowPosition::WIN_POS_CENTER);
        set_resizable(true);
        set_name("nova_browser_window");
        set_border_width(12);

        auto css = Gtk::CssProvider::create();
        css->load_from_data(
            "window#nova_browser_window { background: #eef4f8; color: #142335; }"
            "#browser_root { background: #eef4f8; border-radius: 16px; box-shadow: 0 0 24px rgba(0,0,0,0.28); }"
            "#topbar { background: #ffffff; color: #142335; border-radius: 12px; padding: 4px; }"
            "button { background: linear-gradient(135deg, #dceaff, #eef6ff); color: #173755; border-radius: 10px; padding: 8px 16px; border: 1px solid #b7d4f2; font-weight: bold; transition: background 240ms ease, color 240ms ease, box-shadow 240ms ease; }"
            "button:hover { background: linear-gradient(135deg, #a8d4ff, #eaf6ff); color: #102a47; box-shadow: 0 4px 12px rgba(130,170,220,0.34); }"
            "entry { background: #f7faff; color: #1a2440; border-radius: 10px; padding: 10px; font-size: 14px; border: 1px solid #bfd2e8; transition: box-shadow 260ms ease; }"
            "entry:focus { box-shadow: 0 0 0 3px rgba(130,200,255,0.4); }"
            "label { color: #182d4d; }"
            "#webview_host { background: white; border-radius: 8px; border: 1px solid #d4dce8; }"
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
        m_settings_button.set_label("Settings");

        m_home_button.signal_clicked().connect(sigc::mem_fun(*this, &BrowserWindow::on_home_clicked));
        m_back_button.signal_clicked().connect(sigc::mem_fun(*this, &BrowserWindow::on_back_clicked));
        m_forward_button.signal_clicked().connect(sigc::mem_fun(*this, &BrowserWindow::on_forward_clicked));
        m_refresh_button.signal_clicked().connect(sigc::mem_fun(*this, &BrowserWindow::on_refresh_clicked));
        m_go_button.signal_clicked().connect(sigc::mem_fun(*this, &BrowserWindow::on_go_clicked));
        m_settings_button.signal_clicked().connect(sigc::mem_fun(*this, &BrowserWindow::on_settings_clicked));

        m_topbar.pack_start(m_home_button, Gtk::PACK_SHRINK);
        m_topbar.pack_start(m_back_button, Gtk::PACK_SHRINK);
        m_topbar.pack_start(m_forward_button, Gtk::PACK_SHRINK);
        m_topbar.pack_start(m_refresh_button, Gtk::PACK_SHRINK);
        m_topbar.pack_start(m_address_entry, Gtk::PACK_EXPAND_WIDGET);
        m_topbar.pack_start(m_go_button, Gtk::PACK_SHRINK);
        m_topbar.pack_start(m_settings_button, Gtk::PACK_SHRINK);

        m_address_entry.set_placeholder_text("Search or enter a website");
        m_address_entry.set_text("");
        m_address_entry.signal_activate().connect(sigc::mem_fun(*this, &BrowserWindow::on_go_clicked));

        m_root.pack_start(m_topbar, Gtk::PACK_SHRINK);

        m_content_manager = webkit_user_content_manager_new();
        webkit_user_content_manager_register_script_message_handler(m_content_manager, "nova");
        m_webview = WEBKIT_WEB_VIEW(webkit_web_view_new_with_user_content_manager(m_content_manager));

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
        m_status_url.set_text("https://home.nova.local");

        m_statusbar.pack_start(m_status_icon, Gtk::PACK_SHRINK);
        m_statusbar.pack_start(m_status_text, Gtk::PACK_SHRINK);
        m_statusbar.pack_start(m_status_url, Gtk::PACK_EXPAND_WIDGET);

        m_root.pack_start(m_statusbar, Gtk::PACK_SHRINK);

        add(m_root);
        show_all_children();

        g_signal_connect(m_content_manager, "script-message-received::nova",
                         G_CALLBACK(script_message_received_callback), this);

        load_homepage();

        g_signal_connect(m_webview, "load-changed", G_CALLBACK(load_changed_callback), this);
    }

private:
    void on_go_clicked() {
        std::string input = m_address_entry.get_text();
        if (input.empty()) {
            load_homepage();
            return;
        }

        if (input.find("://") == std::string::npos) {
            std::string q = input;
            std::string url = "https://duckduckgo.com/?q=" + percent_encode(q);
            load_url(url);
        } else {
            load_url(normalize_url(input));
        }
    }

    void on_refresh_clicked() {
        webkit_web_view_reload(m_webview);
        m_status_text.set_text("refreshing");
    }

    void on_home_clicked() {
        load_homepage();
    }

    void on_settings_clicked() {
        load_settings_page();
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
            self->m_status_icon.set_text("●");
        }
    }

    static void script_message_received_callback(WebKitUserContentManager* manager,
                                                 WebKitJavascriptResult* result,
                                                 gpointer user_data) {
        (void)manager;
        BrowserWindow* self = static_cast<BrowserWindow*>(user_data);
        JSCValue* js_value = webkit_javascript_result_get_js_value(result);
        gchar* value = jsc_value_to_string(js_value);
        std::string msg = value ? value : "";
        g_free(value);

        if (msg == "home") {
            self->load_homepage();
        } else if (msg == "settings") {
            self->load_settings_page();
        }
    }

    void load_homepage() {
        std::string html = R"(
<html>
<head>
<title>NOVA Search</title>
<style>
:root { --blue: #1a73e8; --green: #34a853; --text: #202124; --soft: #eef4fa; }
* { box-sizing: border-box; }
body {
  margin: 0;
  font-family: Inter, "Segoe UI", Arial, Helvetica, sans-serif;
  color: var(--text);
  background: radial-gradient(circle at center, #eef4fa 0%, #dce8f6 100%);
}
.screen {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 24px;
  padding: 30px 16px;
}
.logo {
  font-size: 72px;
  font-weight: 900;
  letter-spacing: -3px;
  color: var(--blue);
  text-align: center;
  animation: logoFloat 1400ms ease-in-out infinite alternate;
}
.logo span:nth-child(1) { color: #4285f4; }
.logo span:nth-child(2) { color: #34a853; }
.logo span:nth-child(3) { color: #fbbc05; }
.logo span:nth-child(4) { color: #ea4335; }
.searchbox {
  width: min(680px, 80vw);
  display: flex;
  align-items: center;
  background: white;
  border-radius: 999px;
  box-shadow: 0 4px 20px rgba(0,0,0,0.14);
  padding: 8px 16px;
  border: 1px solid #d9e8ff;
}
.searchbox svg { flex: 0 0 22px; margin-right: 14px; }
.searchbox input {
  flex-grow: 1;
  border: none;
  outline: none;
  font-size: 18px;
  padding: 12px;
  color: var(--text);
  background: transparent;
}
.searchbox button, .buttonrow button {
  background: #eef3f8;
  color: var(--text);
  border: 1px solid #d9e0ea;
  padding: 10px 16px;
  border-radius: 8px;
  font-weight: 700;
  cursor: pointer;
}
.searchbox button { border-radius: 999px; background: #eef3f8; }
.buttonrow {
  display: flex;
  gap: 12px;
  justify-content: center;
  flex-wrap: wrap;
}
.buttonrow button:hover, .searchbox button:hover { background: #dce8f8; }
.shortcuts {
  display: flex;
  gap: 20px;
  color: #4b596d;
  font-size: 14px;
  flex-wrap: wrap;
  justify-content: center;
}
.shortcuts div { padding: 12px; border-radius: 8px; }
.shortcuts div:hover { background: #eaf2ff; }
.searchbox { animation: riseIn 420ms ease both; }
.shortcut-fade { animation: fadeSlide 560ms ease both; }
@keyframes logoFloat { from { transform: translateY(-2px); } to { transform: translateY(-8px); } }
@keyframes riseIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }
@keyframes fadeSlide { from { opacity: 0; transform: translateY(8px); } to { opacity: 1; transform: translateY(0); } }
@media (max-width: 700px) { .logo { font-size: 48px; } }
</style>
</head>
<body>
<div class='screen'>
  <div class='logo'><span>N</span><span>O</span><span>V</span><span>A</span></div>
  <div class='searchbox'>
    <svg width='22' height='22' viewBox='0 0 24 24' fill='none'><path d='M10.8 18.6a7.8 7.8 0 1 1 0-15.6 7.8 7.8 0 0 1 0 15.6z' stroke='#6a7286' stroke-width='2'/><path d='m16.2 16.2 4 4' stroke='#6a7286' stroke-width='2'/></svg>
    <input id='q' type='text' value='' placeholder='Search the web or enter a site' autocomplete='off'/>
    <button onclick='goSearch()'>Search</button>
  </div>
  <div class='buttonrow'>
    <button onclick='goSearch()'>Search</button>
    <button onclick='location.href="https://search.nova.local"'>Feeling Lucky</button>
  </div>
  <div class='shortcuts'>
    <div class='shortcut-fade'>Docs</div><div class='shortcut-fade'>Images</div><div class='shortcut-fade'>News</div><div class='shortcut-fade'>Maps</div>
  </div>
</div>
<script>
function goSearch() {
  var query = document.getElementById('q').value.trim();
  if (!query) return;
  if (query.indexOf('://') < 0 && query.indexOf('.') >= 0) {
    var url = 'https://' + query;
    location.href = url;
  } else {
    var url = 'https://duckduckgo.com/?q=' + encodeURIComponent(query);
    location.href = url;
  }
}
</script>
</body>
</html>
        )";

        webkit_web_view_load_html(m_webview, html.c_str(), "https://home.nova.local");
        m_status_text.set_text("home");
        m_status_url.set_text("https://home.nova.local");
    }

    void load_settings_page() {
        std::string html = R"(
<html>
<head>
<title>NOVA Settings</title>
<style>
body {
  margin: 0;
  font-family: "Segoe UI", Arial, sans-serif;
  background: #eef3f8;
  color: #20304d;
}
.app {
  width: min(920px, calc(100vw - 80px));
  margin: 38px auto;
  background: linear-gradient(180deg, #ffffff, #eef4fb);
  border-radius: 24px;
  box-shadow: 0 10px 30px rgba(0,0,0,0.16);
  padding: 30px;
}
.header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  border-bottom: 1px solid #ccd8ee;
  padding-bottom: 16px;
}
.title { font-size: 36px; font-weight: 800; color: #172a4a; }
.badge { background: #accef5; color: #102b4d; font-size: 12px; font-weight: 800; padding: 8px 12px; border-radius: 99px; }
.grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 14px;
  padding-top: 20px;
}
.card {
  background: #ffffff;
  border: 1px solid #d4e1ef;
  border-radius: 16px;
  padding: 20px;
}
.card h3 { margin: 0 0 8px; font-size: 16px; color: #193353; }
.card p { margin: 0; color: #65748b; font-size: 13px; }
.toggle { display: flex; align-items: center; gap: 10px; margin-top: 16px; }
.row { display: flex; justify-content: space-between; align-items: center; margin-top: 14px; font-size: 13px; color: #33435d; }
button { background: linear-gradient(135deg, #244a8a, #162b45); color: #fff; border: none; border-radius: 10px; padding: 10px 16px; font-weight: 700; cursor: pointer; }
</style>
</head>
<body>
<div class='app'>
  <div class='header'>
    <div class='title'>NOVA Settings</div>
    <div class='badge'>SYNC ON</div>
  </div>
  <div class='grid'>
    <div class='card'>
      <h3>Search</h3>
      <p>Default search engine</p>
      <div class='row'><span>DuckDuckGo</span><span>✓</span></div>
    </div>
    <div class='card'>
      <h3>Privacy</h3>
      <p>Block trackers</p>
      <div class='toggle'><input type='checkbox' checked> Enhanced privacy</div>
    </div>
    <div class='card'>
      <h3>Appearance</h3>
      <p>Theme and contrast</p>
      <div class='row'><span>Light</span><span>→</span></div>
    </div>
    <div class='card'>
      <h3>Security</h3>
      <p>Safe browsing</p>
      <div class='toggle'><input type='checkbox' checked> Web protection</div>
    </div>
  </div>
  <div style='margin-top: 24px;'><button onclick='window.webkit.messageHandlers.nova.postMessage("home")'>Back to Browser</button></div>
</div>
</body>
</html>
        )";

        webkit_web_view_load_html(m_webview, html.c_str(), "https://settings.nova.local");
        m_status_text.set_text("settings");
        m_status_url.set_text("https://settings.nova.local");
    }

    void load_url(const std::string& raw) {
        std::string url = normalize_url(raw);
        m_address_entry.set_text(url);
        webkit_web_view_load_uri(m_webview, url.c_str());
        m_status_text.set_text("loading");
        m_status_url.set_text(url);
    }

    std::string normalize_url(const std::string& raw) {
        std::string url = raw;
        if (url.find("://") == std::string::npos) {
            url = "https://" + url;
        }
        return url;
    }

    std::string percent_encode(const std::string& raw) {
        std::string out;
        for (unsigned char c : raw) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
                out.push_back(c);
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", c);
                out += buf;
            }
        }
        return out;
    }

    Gtk::Box m_root;
    Gtk::Box m_topbar;
    Gtk::Box m_statusbar;
    Gtk::Box m_search_box;
    Gtk::Entry m_address_entry;
    Gtk::Button m_home_button;
    Gtk::Button m_back_button;
    Gtk::Button m_forward_button;
    Gtk::Button m_refresh_button;
    Gtk::Button m_go_button;
    Gtk::Button m_settings_button;
    Gtk::Label m_status_icon;
    Gtk::Label m_status_text;
    Gtk::Label m_status_url;
    Gtk::ScrolledWindow m_web_scroll;
    WebKitUserContentManager* m_content_manager = nullptr;
    WebKitWebView* m_webview;
};

int main(int argc, char** argv) {
    auto app = Gtk::Application::create(argc, argv, "com.nova.browser");
    BrowserWindow win;
    return app->run(win);
}
