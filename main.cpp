#include "httplib.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <chrono>
#include <mutex>

using namespace std;

struct RateLimitInfo {
    int count = 0;
    chrono::steady_clock::time_point reset_time;
};

static map<string, RateLimitInfo> ip_tracker;
static mutex rate_limit_mtx;
const int MAX_REQUESTS_PER_MINUTE = 12;

static int safe_stoi(const string& s, int default_val = 0) {
    try { if (s.empty()) return default_val; return stoi(s); } catch (...) { return default_val; }
}
static float safe_stof(const string& s, float default_val = 0.0f) {
    try { if (s.empty()) return default_val; return stof(s); } catch (...) { return default_val; }
}
static int clamp_int(int v, int lo, int hi) { return max(lo, min(hi, v)); }
static float clamp_float(float v, float lo, float hi) { return max(lo, min(hi, v)); }

static string html_escape(const string& data) {
    string buffer; buffer.reserve(data.size());
    for (size_t pos = 0; pos != data.size(); ++pos) {
        switch (data[pos]) {
            case '&':  buffer.append("&amp;");       break;
            case '\"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&apos;");      break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    return buffer;
}

static void set_security_headers(httplib::Response& res) {
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-XSS-Protection", "1; mode=block");
    res.set_header("Content-Security-Policy", "default-src 'self' https://fonts.googleapis.com https://fonts.gstatic.com; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; font-src https://fonts.gstatic.com;");
    res.set_header("Server", "Hammer-Engine/1.0"); 
}

static string get_client_ip(const httplib::Request& req) {
    if (req.has_header("CF-Connecting-IP")) return req.get_header_value("CF-Connecting-IP");
    if (req.has_header("X-Forwarded-For")) {
        string xff = req.get_header_value("X-Forwarded-For");
        size_t comma = xff.find(',');
        if (comma != string::npos) return xff.substr(0, comma);
        return xff;
    }
    return req.remote_addr;
}

static bool is_rate_limited(const string& ip) {
    lock_guard<mutex> lock(rate_limit_mtx);
    auto now = chrono::steady_clock::now();
    if (ip_tracker.size() > 500) {
        for (auto it = ip_tracker.begin(); it != ip_tracker.end(); ) {
            if (now >= it->second.reset_time) it = ip_tracker.erase(it); else ++it;
        }
    }
    if (ip_tracker.find(ip) == ip_tracker.end() || now >= ip_tracker[ip].reset_time) {
        ip_tracker[ip].count = 1; ip_tracker[ip].reset_time = now + chrono::minutes(1); return false;
    }
    ip_tracker[ip].count++;
    if (ip_tracker[ip].count > MAX_REQUESTS_PER_MINUTE) return true;
    return false;
}

static void send_rate_limit_error(httplib::Response& res) {
    ostringstream os;
    os << "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
       << "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
       << "<style>"
       << "body{background-color:#0f172a; font-family:'Cairo', sans-serif; display:flex; align-items:center; justify-content:center; min-height:100vh; margin:0; direction:rtl;}"
       << ".limit-card{background:#1e293b; border:1px solid #f59e0b; padding:40px; border-radius:16px; text-align:center; max-width:500px; width:90%; box-shadow:0 20px 25px -5px rgba(0,0,0,0.3);}"
       << ".limit-card h2{color:#f59e0b; font-size:22px; margin-top:0; font-weight:700;}"
       << ".limit-card p{color:#94a3b8; font-size:15px; line-height:1.7; margin-bottom:0;}"
       << "</style></head><body>"
       << "<div class='limit-card'>"
       << "<h2>⚠️ تم تجاوز حد الطلبات المسموح به</h2>"
       << "<p>لقد أرسلت عدداً كبيراً من الطلبات في وقت قصير (الحد الأقصى 12 طلباً في الدقيقة).<br>يرجى الانتظار دقيقة واحدة ثم إعادة المحاولة بشكل طبيعي.</p>"
       << "</div></body></html>";
    res.status = 429; res.set_content(os.str(), "text/html; charset=utf-8");
}

class Elevator {
private:
    const float P_BRACKET = 0.0f; const float P_BOLT = 0.0f; const float P_ROPE = 0.0f; const float P_FISH = 0.0f; const float P_RAIL = 0.0f;
public:
    string get_door_type(int sa) {
        if (sa >= 210 && sa <= 250)      return "Auto 80 CO || Auto 90 CO || Auto 100 CO";
        else if (sa >= 190 && sa < 210)  return "Auto 80 CO || Auto 90 CO || Auto 100 SI";
        else if (sa >= 190 && sa < 195)  return "Auto 80 CO || Auto 90 CO || Auto 100 SI";
        else if (sa >= 175 && sa < 190)  return "Auto 80 CO || Auto 100 SI || Auto 90 SI";
        else if (sa >= 167 && sa < 175)  return "Auto 90 SI || Auto 80 CO";
        else if (sa >= 160 && sa < 167)  return "Auto 90 SI || Auto 70 CO";
        else if (sa >= 155 && sa < 160)  return "Auto 80 SI || Auto 70 CO";
        else if (sa >= 145 && sa < 155)  return "Auto 80 SI";
        else if (sa >= 128 && sa < 145)  return "Auto 70 SI";
        else if (sa >= 120 && sa < 128)  return "Semi Auto 80";
        else if (sa >= 110 && sa < 120)  return "Semi Auto 70";
        return "No standard door";
    }
    int get_cabin_dbg(int w) { return w - 30; }
    int get_cwt_dbg(int v) {
        if (v >= 100 && v <= 110) return 72; if (v > 110 && v <= 120) return 82;
        if (v > 120 && v <= 125) return 92;  if (v > 125 && v <= 210) return 102; return 0;
    }
    int get_cabin_width(int cw) { return cw - 40; }
    int get_cabin_depth(int cd) { return cd - 60; }
    float get_shaft_height(float f, float pit_m, float overhead_m, string t) { 
        float typical_floors_height = (f - 1) * 3.2f; float total_h = typical_floors_height + pit_m + overhead_m;
        return (t == "MRL") ? total_h + 1.5f : total_h; 
    }
    int calc_brackets(float h) { return (h / 2.0) * 4; }
    int calc_bolts(int b) { return b * 4; }
    float calc_ropes(float h) { return ((h * 2) + 5) * 4; }
    float get_p_bracket() { return P_BRACKET; } float get_p_bolt() { return P_BOLT; } 
    float get_p_rope() { return P_ROPE; } float get_p_fish() { return P_FISH; } float get_p_rail() { return P_RAIL; }
};

int main() {
    httplib::Server svr; Elevator elevator;

    // 1️⃣ البوابة الرئيسية (تصميم Dark Mode احترافي مريح جداً)
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        set_security_headers(res);
        string client_ip = get_client_ip(req);
        if (is_rate_limited(client_ip)) { send_rate_limit_error(res); return; }

        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;500;600;700&display=swap' rel='stylesheet'>"
                      "<style>"
                      "body{background-color:#0f172a; font-family:'Cairo', sans-serif; color:#f8fafc; direction:rtl; padding:30px 15px; display:flex; flex-direction:column; align-items:center; min-height:100vh; margin:0; box-sizing:border-box;}"
                      "header{text-align:center; margin-bottom:35px;}"
                      "header h1{color:#f59e0b; font-size:2.8rem; margin:0; font-weight:800; text-shadow: 0 4px 12px rgba(245,158,11,0.15);}"
                      "header p{color:#94a3b8; font-size:1.1rem; margin-top:8px; font-weight:500;}"
                      ".alert-box{background:rgba(239,68,68,0.07); border:1px dashed rgba(239,68,68,0.4); padding:20px; border-radius:14px; max-width:800px; width:100%; text-align:center; margin-bottom:35px; box-sizing:border-box;}"
                      ".alert-box h4{color:#f87171; margin:0 0 6px 0; font-size:1.1rem; font-weight:700;}"
                      ".alert-box p{color:#cbd5e1; margin:0; font-size:0.95rem; line-height:1.65; font-weight:500;}"
                      ".grid-nav{display:grid; grid-template-columns:repeat(auto-fit, minmax(280px, 1fr)); gap:25px; width:100%; max-width:850px;}"
                      ".nav-card{background:#1e293b; border:1px solid #334155; padding:30px; border-radius:16px; text-decoration:none; color:#f8fafc; transition:all 0.3s cubic-bezier(0.4, 0, 0.2, 1); display:flex; flex-direction:column; box-shadow:0 4px 6px -1px rgba(0,0,0,0.1);}"
                      ".nav-card:hover{border-color:#f59e0b; transform:translateY(-4px); box-shadow:0 20px 25px -5px rgba(0,0,0,0.25), 0 0 15px rgba(245,158,11,0.1);}"
                      ".nav-card h3{color:#f59e0b; font-size:1.35rem; margin:0 0 10px 0; font-weight:700;}"
                      ".nav-card p{color:#94a3b8; font-size:0.92rem; line-height:1.6; margin:0; font-weight:400;}"
                      ".disabled{opacity:0.4; cursor:not-allowed;}"
                      ".disabled:hover{transform:none; border-color:#334155; box-shadow:none;}"
                      ".footer{margin-top:auto; padding-top:50px; font-size:13px; color:#475569; text-align:center; font-weight:600; letter-spacing:0.5px;}"
                      "</style></head><body>"
                      "<header>"
                      "<h1>ضربة شاكوش 🛠️</h1>"
                      "<p>المنصة الهندسية المتكاملة لتقنيات المصاعد والتحكم الآلي</p>"
                      "</header>"
                      "<div class='alert-box'>"
                      "<h4>⚠️ تنبيه تنظيمي هام جداً</h4>"
                      "<p>المنصة حالياً في مرحلتها التجريبية الرابعة والتطوير البرمجي مستمر. يمكنك استخدام الحاسبة ومراجعة النتائج مجاناً، ولكن يرجى عدم الاعتماد النهائي والمطلق على المقاسات الناتجة دون مراجعتها ومطابقتها فنيّاً وهندسيّاً في أرض الموقع.</p>"
                      "</div>"
                      "<div class='grid-nav'>"
                      "<a href='/calculator' class='nav-card'><h3>🛗 حاسبة المقاسات والبضاعة</h3><p>تصفية أبعاد بئر المصعد وحساب الكابينة والمواد الهندسية المطلوبة فوراً.</p></a>"
                      "<a href='/blog' class='nav-card'><h3>📚 المقالات والشروحات العلمية</h3><p>شروحات صيانة الكروت الإلكترونية، بروتوكولات الأجهزة، وبرمجة متحكمات الروبوت.</p></a>"
                      "<div class='nav-card disabled'><h3>🦾 واجهات التحكم والمحاور</h3><p>(قريباً) نظام متطور لحساب المعاملات الحركية ومحاور ماكينات الـ CNC بالـ C++.</p></div>"
                      "</div>"
                      "<div class='footer'>تطوير وهندسة: محمد الشعراوي © 2026</div>"
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 2️⃣ واجهة إدخال بيانات الحاسبة (نظيفة، مريحة، بألوان راقية)
    svr.Get("/calculator", [](const httplib::Request& req, httplib::Response& res) {
        set_security_headers(res);
        string client_ip = get_client_ip(req);
        if (is_rate_limited(client_ip)) { send_rate_limit_error(res); return; }

        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
                      "<style>"
                      "body{background-color:#f8fafc; font-family:'Cairo', sans-serif; display:flex; align-items:center; justify-content:center; min-height:100vh; margin:0; padding:20px; box-sizing:border-box; flex-direction:column; color:#1e293b;}"
                      ".card{background:#ffffff; padding:40px 35px; border-radius:24px; box-shadow: 0 25px 50px -12px rgba(15,23,42,0.08); width:100%; max-width:560px; direction:rtl; text-align:right; box-sizing:border-box; border:1px solid #e2e8f0;}"
                      "h2{color:#0f172a; text-align:center; margin:0 0 8px 0; font-weight:800; font-size:24px;}"
                      ".sub-title{text-align:center; color:#64748b; margin-bottom:35px; font-size:14px; font-weight:500;}"
                      ".f-group{margin-bottom:24px;}"
                      "label{font-weight:600; color:#334155; display:block; margin-bottom:8px; font-size:14px;}"
                      "input,select{width:100%; padding:14px; border:1px solid #cbd5e1; border-radius:12px; box-sizing:border-box; text-align:center; font-size:15px; font-family:'Cairo', sans-serif; background-color:#f1f5f9; color:#1e293b; transition:all 0.2s ease; font-weight:600;}"
                      "input:focus, select:focus{outline:none; border-color:#2563eb; background-color:#fff; box-shadow:0 0 0 4px rgba(37,99,235,0.1);}"
                      "button{background:linear-gradient(135deg, #1e293b, #0f172a); color:white; border:none; padding:16px; border-radius:12px; width:100%; font-size:16px; font-weight:700; font-family:'Cairo', sans-serif; cursor:pointer; margin-top:10px; box-shadow:0 10px 15px -3px rgba(15,23,42,0.2); transition:all 0.2s;}"
                      "button:hover{transform:translateY(-1px); box-shadow:0 20px 25px -5px rgba(15,23,42,0.25); opacity:0.95;}"
                      ".btn-home{display:block; text-align:center; margin-top:24px; color:#2563eb; text-decoration:none; font-weight:700; font-size:14px; transition:color 0.2s;}"
                      ".btn-home:hover{color:#1d4ed8;}"
                      "</style></head><body>"
                      "<div class='card'><h2>🧮 حاسبة المقاسات والبضاعة الذكية</h2>"
                      "<div class='sub-title'>النظام الحسابي المطور لشركات ومؤسسات تركيب وتصفية المصاعد</div>"
                      "<form action='/calculate' method='post'>"
                      "<div class='f-group'><label>📦 نوع النظام الهندسي للمصعد:</label><select name='m_type'><option value='MR'>غرفة محرك أعلى البئر (MR)</option><option value='MRL'>بدون غرفة محرك (MRL)</option></select></div>"
                      "<div class='f-group'><label>📏 عرض بئر المصعد الحُر (CM):</label><input type='number' name='width' required min='80' max='250' placeholder='أدخل عرض البئر بالسم'></div>"
                      "<div class='f-group'><label>📐 عمق بئر المصعد الحُر (CM):</label><input type='number' name='depth' required min='80' max='250' placeholder='أدخل عمق البئر بالسم'></div>"
                      "<div class='f-group'><label>🏢 عدد وقفات المبنى (الأدوار):</label><input type='number' name='floors' required min='1' max='60' placeholder='أدخل إجمالي الأدوار'></div>"
                      "<div class='f-group'><label>🕳️ عمق الحفرة السفلي Pit (CM):</label><input type='number' name='depth_pit' required min='10' max='500' value='100'></div>"
                      "<div class='f-group'><label>🏠 الارتفاع العلوي الأخير Overhead (CM):</label><input type='number' name='overhead' required min='100' max='800' value='400'></div>"
                      "<button type='submit'>🚀 تحليل الأبعاد واستخراج المقايسة</button></form>"
                      "<a href='/' class='btn-home'>⬅️ العودة للبوابة الرئيسية</a></div>"
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 3️⃣ صفحة تقرير المقايسة (تصميم مريح للعين، نظيف جداً، ومناسب تماماً للطباعة)
    svr.Post("/calculate", [&elevator](const httplib::Request& req, httplib::Response& res) {
        set_security_headers(res);
        string client_ip = get_client_ip(req);
        if (is_rate_limited(client_ip)) { send_rate_limit_error(res); return; }
        
        string m_type = html_escape(req.get_param_value("m_type"));
        if (m_type != "MR" && m_type != "MRL") { m_type = "MR"; }

        int w = safe_stoi(req.get_param_value("width"), 0);
        int d = safe_stoi(req.get_param_value("depth"), 0);
        float f = safe_stof(req.get_param_value("floors"), 0.0f);
        int original_pit = safe_stoi(req.get_param_value("depth_pit"), 100);
        int original_overhead = safe_stoi(req.get_param_value("overhead"), 400);

        if (w < 110 || d < 100) {
            ostringstream error_os;
            error_os << "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                     << "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
                     << "<style>"
                     << "body{background-color:#f8fafc; font-family:'Cairo', sans-serif; display:flex; align-items:center; justify-content:center; min-height:100vh; margin:0; direction:rtl;}"
                     << ".error-card{background:#ffffff; border:1px solid #e2e8f0; border-top:4px solid #ea580c; padding:40px; border-radius:16px; text-align:center; max-width:520px; width:90%; box-shadow:0 20px 25px -5px rgba(0,0,0,0.05);}"
                     << ".error-card h2{color:#ea580c; font-size:22px; margin-top:0; font-weight:700;}"
                     << ".error-card p{color:#475569; font-size:15px; line-height:1.7; margin-bottom:25px;}"
                     << ".btn-retry{display:inline-block; background:#ea580c; color:white; padding:12px 30px; text-decoration:none; border-radius:10px; font-weight:700; transition:background 0.2s;}"
                     << ".btn-retry:hover{background:#c2410c;}"
                     << "</style></head><body>"
                     << "<div class='error-card'>"
                     << "<h2>⚠️ أبعاد البئر غير مطابقة للمواصفات الفنية</h2>"
                     << "<p>أبعاد بئر المصعد المدخلة (العرض: " << w << " سم، العمق: " << d << " سم) أقل من الحد الأدنى للحساب التلقائي بالمنصة.<br><b>المواصفات المطلوبة:</b> العرض لا يقل عن 110 سم، والعمق لا يقل عن 100 سم.</p>"
                     << "<a href='/calculator' class='btn-retry'>🔄 تعديل المقاسات</a>"
                     << "</div></body></html>";
            res.set_content(error_os.str(), "text/html; charset=utf-8"); return;
        }

        string pit_display_text, overhead_display_text;
        if (original_pit < 60 || original_pit > 200) {
            pit_display_text = "<span style='color:#ef4444; background:#fef2f2; padding:3px 8px; border-radius:6px; border:1px solid #fee2e2; font-size:13px; font-weight:700;'>⚠️ غير قياسي (" + to_string(original_pit) + " CM)</span>";
        } else { pit_display_text = to_string(original_pit) + " CM"; }

        if (original_overhead < 350 || original_overhead > 600) {
            overhead_display_text = "<span style='color:#ef4444; background:#fef2f2; padding:3px 8px; border-radius:6px; border:1px solid #fee2e2; font-size:13px; font-weight:700;'>⚠️ غير قياسي (" + to_string(original_overhead) + " CM)</span>";
        } else { overhead_display_text = to_string(original_overhead) + " CM"; }

        w = clamp_int(w, 80, 250); d = clamp_int(d, 80, 250); f = clamp_float(f, 1.0f, 60.0f);
        float pit_clamped = clamp_float((float)original_pit, 10.0f, 500.0f);
        float overhead_clamped = clamp_float((float)original_overhead, 100.0f, 800.0f);

        float pit_m = pit_clamped / 100.0f; float overhead_m = overhead_clamped / 100.0f;
        string door = elevator.get_door_type(w); int cabin_dbg = elevator.get_cabin_dbg(w); int cwt_dbg = elevator.get_cwt_dbg(w);
        int cab_w = elevator.get_cabin_width(w); int cab_d = elevator.get_cabin_depth(d);
        float h = elevator.get_shaft_height(f, pit_m, overhead_m, m_type);

        int brackets = elevator.calc_brackets(h); int bolts = elevator.calc_bolts(brackets); float ropes = elevator.calc_ropes(h);
        int fishplates = ((int)f) * 4; float rail_qty = (h * 4) / 5.0f; 

        float c_brackets = brackets * elevator.get_p_bracket(); float c_bolts = bolts * elevator.get_p_bolt(); 
        float c_ropes = ropes * elevator.get_p_rope(); float c_fishplates = fishplates * elevator.get_p_fish(); float c_rail = rail_qty * elevator.get_p_rail();
        float total = c_brackets + c_bolts + c_ropes + c_fishplates + c_rail;

        string cwt_display_text;
        if (cwt_dbg == 0) {
            cwt_display_text = "<span style='color:#ef4444; background:#fef2f2; padding:3px 8px; border-radius:6px; border:1px solid #fee2e2; font-size:13px; font-weight:700;'>⚠️ مراجعة يدوية</span>";
        } else { cwt_display_text = to_string(cwt_dbg) + " CM"; }

        ostringstream os;
        os << "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
           << "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
           << "<style>"
           << "body{background-color:#f1f5f9; font-family:'Cairo', sans-serif; padding:40px 15px; direction:rtl; text-align:right; color:#334155; margin:0;}"
           << ".box{max-width:700px; margin:auto; background:#ffffff; padding:40px; border-radius:24px; box-shadow:0 20px 25px -5px rgba(0,0,0,0.05); border:1px solid #e2e8f0;}"
           << "h2{color:#0f172a; text-align:center; margin:0 font-weight:800; font-size:24px; border-bottom:2px solid #f1f5f9; padding-bottom:18px;}"
           << "h3{color:#1e3a8a; font-size:16px; font-weight:700; margin-top:30px; margin-bottom:12px; display:flex; align-items:center; gap:8px;}"
           << ".table-container{width:100%; overflow-x:auto; background:#fff; border-radius:12px; border:1px solid #e2e8f0; margin-top:8px;}"
           << ".tbl{width:100%; border-collapse:collapse; text-align:right;}"
           << ".tbl th{background:#f8fafc; padding:14px 18px; color:#475569; font-weight:600; border-bottom:1px solid #e2e8f0; font-size:14px; width:45%;}"
           << ".tbl td{padding:14px 18px; border-bottom:1px solid #e2e8f0; color:#0f172a; font-size:14px; font-weight:700;}"
           << ".btbl{width:100%; border-collapse:collapse; text-align:center;}"
           << ".btbl th{background:#1e293b; color:white; padding:14px; font-weight:600; font-size:13px;}"
           << ".btbl td{padding:14px; border-bottom:1px solid #e2e8f0; color:#334155; font-size:14px; font-weight:700;}"
           << ".btbl tr:nth-child(even){background-color:#f8fafc;}"
           << ".inv{background:linear-gradient(135deg, #f0fdf4, #dcfce7); padding:20px; border-radius:14px; border:1px dashed #16a34a; margin-top:30px; text-align:center; font-size:19px; font-weight:800; color:#14532d; box-shadow:0 4px 12px rgba(22,163,74,0.05);}"
           << ".actions{display:flex; justify-content:space-between; margin-top:35px; gap:15px;}"
           << ".btn-print{background:#16a34a; color:white; padding:14px 25px; border:none; border-radius:12px; font-weight:700; font-family:'Cairo', sans-serif; cursor:pointer; font-size:14px; flex:1; box-shadow:0 4px 12px rgba(22,163,74,0.15); transition:all 0.2s;}"
           << ".btn-print:hover{background:#15803d;}"
           << ".btn-back{background:#2563eb; color:white; padding:14px 25px; text-decoration:none; border-radius:12px; font-weight:700; font-size:14px; text-align:center; flex:1; box-shadow:0 4px 12px rgba(37,99,235,0.15); transition:all 0.2s;}"
           << ".btn-back:hover{background:#1d4ed8;}"
           << "@media print{.btn-print, .btn-back, .actions {display:none;} .box{box-shadow:none; padding:0; border:none; body{background:#fff;}}}"
           << "</style></head><body><div class='box'><h2>📋 تقرير تصفية الأبعاد الفنية والمقايسة</h2>"
           << "<h3>📐 أولاً: البيانات الهندسية الناتجة</h3>"
           << "<div class='table-container'><table class='tbl'>"
           << "<tr><th>نوع الباب الافتراضي للموقع:</th><td style='color:#2563eb;'>" << door << "</td></tr>"
           << "<tr><th>مقاس DBG الكابينة:</th><td>" << cabin_dbg << " CM</td></tr>"
           << "<tr><th>مقاس DBG الثقل (CWT):</th><td>" << cwt_display_text << "</td></tr>"
           << "<tr><th>صافي عرض الكابينة الداخلي:</th><td>" << cab_w << " CM</td></tr>"
           << "<tr><th>صافي عمق الكابينة الداخلي:</th><td>" << cab_d << " CM</td></tr>"
           << "<tr><th>مقاس عمق الحفرة المدخل:</th><td>" << pit_display_text << "</td></tr>"
           << "<tr><th>مقاس الارتفاع العلوي المدخل:</th><td>" << overhead_display_text << "</td></tr>"
           << "<tr><th>إجمالي مشوار البئر المحسوب:</th><td style='color:#ea580c;'>" << h << " متر</td></tr>"
           << "</table></div>"
           << "<h3>📦 ثانياً: كمية البضاعة المحسوبة للمشوار</h3>"
           << "<div class='table-container'><table class='btbl'><thead><tr><th>اسم الصنف ومواصفاته الفنية</th><th>الكمية المطلوبة</th><th>التكلفة التقديرية</th></tr></thead><tbody>"
           << "<tr><td>كوابيل السكك الحديدية</td><td>" << brackets << " قطعة 🛑</td><td>" << c_brackets << " SAR</td></tr>"
           << "<tr><td>مسامير وجوايط التثبيت للآبار</td><td>" << bolts << " مسمار 🔩</td><td>" << c_bolts << " SAR</td></tr>"
           << "<tr><td>حبال واير الفولاذ القياسية</td><td>" << ropes << " متر 🧵</td><td>" << c_ropes << " SAR</td></tr>"
           << "<tr><td>لقم ربط السكك (التقفيل)</td><td>" << fishplates << " لقمة 🗜️</td><td>" << c_fishplates << " SAR</td></tr>"
           << "<tr><td>قضبان السكك الحديدية (الريل)</td><td>" << rail_qty << " قضيب (5م) 🛤️</td><td>" << c_rail << " SAR</td></tr>"
           << "</tbody></table></div>"
           << "<div class='inv'>💰 إجمالي القيمة المالية التقديرية: " << total << " SAR</div>"
           << "<div class='actions'>"
           << "<button class='btn-print' onclick='window.print()'>🖨️ طباعة التقرير الفني / حفظ PDF</button>"
           << "<a class='btn-back' href='/calculator'>🔄 حساب مقايسة جديدة</a>"
           << "</div></div></body></html>";
        res.set_content(os.str(), "text/html; charset=utf-8");
    });

    // 4️⃣ مسار المقالات (تصميم Dark Mode مريح للقراءة وشاشات التابلت)
    svr.Get("/blog", [](const httplib::Request& req, httplib::Response& res) {
        set_security_headers(res);
        string client_ip = get_client_ip(req);
        if (is_rate_limited(client_ip)) { send_rate_limit_error(res); return; }

        string blog_html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                           "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
                           "<style>"
                           "body{background-color:#0f172a; font-family:'Cairo', sans-serif; color:#f8fafc; direction:rtl; padding:40px 20px; margin:0; box-sizing:border-box; display:flex; flex-direction:column; align-items:center; min-height:100vh;}"
                           ".container{max-width:800px; width:100%;}"
                           "h1{color:#f59e0b; border-bottom:2px solid #334155; padding-bottom:15px; font-weight:800; font-size:2rem; margin:0 0 20px 0;}"
                           ".card{background:#1e293b; padding:25px; border-radius:16px; margin-top:20px; border:1px solid #334155; box-shadow:0 4px 6px -1px rgba(0,0,0,0.1);}"
                           "h2{color:#f59e0b; font-size:1.3rem; margin:0 0 10px 0; font-weight:700;}"
                           "p{color:#94a3b8; line-height:1.7; margin:0; font-size:0.95rem;}"
                           ".btn-back{display:inline-block; margin-top:30px; background:#2563eb; color:white; padding:12px 28px; text-decoration:none; border-radius:50px; font-weight:700; font-size:14px; box-shadow:0 4px 12px rgba(37,99,235,0.2); transition:background 0.2s;}"
                           ".btn-back:hover{background:#1d4ed8;}"
                           "</style></head><body><div class='container'>"
                           "<h1>📚 بوابة ضربة شاكوش للمقالات والشروحات الهندسية</h1>"
                           "<div class='card'><h2>قريباً: شرح مخططات DWG والتثبيت الميكانيكي للمصاعد</h2><p>هنا سيتم رفع الشروحات الفنية المفصلة لطرق تركيب وتوزيع السكك، والمقاسات القياسية لكوابين المصاعد بمختلف أنواعها جيرلس وهيدروليك وتوزيع الأحمال الفنية...</p></div>"
                           "<div class='card'><h2>قريباً: التحكم البرمجي بالـ C++ وكروت صيانة الروبوتات</h2><p>شرح عملي عميق لكيفية بناء الدوال الحسابية وتحويل الأوامر المدخلة إلى إشارات ميكانيكية دقيقة للـ CNC ومحركات التوجيه الذكية والـ Arduino...</p></div>"
                           "<center><a class='btn-back' href='/'>🧮 العودة للبوابة الرئيسية</a></center>"
                           "</div></body></html>";
        res.set_content(blog_html, "text/html; charset=utf-8");
    });

    const char* port_env = getenv("PORT");
    int port = port_env ? safe_stoi(port_env, 8080) : 8080;
    svr.listen("0.0.0.0", port);
    return 0;
}
