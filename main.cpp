#include "httplib.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <nlohmann/json.hpp> // مكتبة التعامل الآمن مع البيانات للمستقبل

using namespace std;
using json = nlohmann::json;

// ============================================================
//  دوال تحويل وحماية آمنة: تمنع توقف السيرفر عند التلاعب بالمدخلات
// ============================================================
static int safe_stoi(const string& s, int default_val = 0) {
    try {
        if (s.empty()) return default_val;
        return stoi(s);
    } catch (...) {
        return default_val;
    }
}

static float safe_stof(const string& s, float default_val = 0.0f) {
    try {
        if (s.empty()) return default_val;
        return stof(s);
    } catch (...) {
        return default_val;
    }
}

static int clamp_int(int v, int lo, int hi) {
    return max(lo, min(hi, v));
}

static float clamp_float(float v, float lo, float hi) {
    return max(lo, min(hi, v));
}

// دالة الحماية من حقن النصوص الخبيثة (XSS)
static string html_escape(const string& data) {
    string buffer;
    buffer.reserve(data.size());
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

// ============================================================
//  كلاس المصعد: يحتوي على المعادلات الثابتة وأسعار السوق (شغلك الأصلي)
// ============================================================
class Elevator {
private:
    const float P_BRACKET = 150.0;
    const float P_BOLT = 25.0;
    const float P_ROPE = 80.0;
    const float P_FISH = 45.0;
    const float P_RAIL = 1200.0; // سعر قضيب الريل الواحد (طول 5 متر)

public:
    string get_door_type(int sa) {
        if (sa >= 191 && sa <= 210) return "Auto 90 CO";
        else if (sa > 168 && sa <= 190)  return "Auto 80 CO";
        else if (sa >= 157 && sa <= 168) return "Auto 70 CO";
        else if (sa >= 175 && sa <= 200) return "Auto 100 SI";
        else if (sa >= 158 && sa <= 175) return "Auto 90 SI";
        else if (sa >= 144 && sa <= 160) return "Auto 80 SI";
        else if (sa >= 128 && sa < 144)  return "Auto 70 SI";
        else if (sa >= 121 && sa <= 135) return "Semi Auto 80";
        else if (sa >= 105 && sa <= 120) return "Semi Auto 70";
        return "No standard door";
    }

    int get_cabin_dbg(int w) { return w - 30; }
    int get_cwt_dbg(int v) {
        if (v >= 100 && v <= 110) return 72;
        if (v > 110 && v <= 120) return 82;
        if (v > 120 && v <= 125) return 92;
        if (v > 125 && v <= 210) return 102;
        return 0;
    }
    int get_cabin_width(int cw) { return cw - 40; }
    int get_cabin_depth(int cd) { return cd - 60; }
    float get_shaft_height(float f, string t) { return (t == "MRL") ? (f * 4) + 1.5 : (f * 4); }

    int calc_brackets(float h) { return (h / 2.0) * 4; }
    int calc_bolts(int b) { return b * 4; }
    float calc_ropes(float h) { return ((h * 2) + 5) * 4; }

    float get_p_bracket() { return P_BRACKET; }
    float get_p_bold() { return P_BOLT; }
    float get_p_rope() { return P_ROPE; }
    float get_p_fish() { return P_FISH; }
    float get_p_rail() { return P_RAIL; }
};

// ============================================================
//  الدالة الرئيسية وتشغيل السيرفر
// ============================================================
int main() {
    httplib::Server svr;
    Elevator elevator;

    // 1️⃣ الصفحة الرئيسية الحالية للحاسبة (بدون أي تعديل على واجهتك)
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
                      "<style>"
                      "body{background-color:#f4f7fc; font-family:'Cairo', sans-serif; display:flex; align-items:center; justify-content:center; min-height:100vh; margin:0; padding:20px; box-sizing:border-box; flex-direction:column; color:#2d3748;}"
                      ".yt-banner{text-align:center; margin-bottom:20px; transition: transform 0.3s;}"
                      ".yt-banner:hover{transform: scale(1.03);}"
                      ".yt-banner img{width:75px; height:75px; border-radius:50%; box-shadow:0 4px 15px rgba(255,0,0,0.15); border:3px solid #ff0000;}"
                      ".yt-banner a{display:block; color:#ff0000; text-decoration:none; font-weight:700; margin-top:8px; font-size:15px; letter-spacing:0.5px;}"
                      ".card{background: #ffffff; padding:40px; border-radius:20px; box-shadow: 0 10px 30px rgba(160, 174, 192, 0.2); width:95%; max-width:550px; direction:rtl; text-align:right; box-sizing:border-box; border: 1px solid rgba(226, 232, 240, 0.8);}"
                      "h2{color:#1a365d; text-align:center; margin-top:0; margin-bottom:10px; font-weight:700; font-size:24px;}"
                      ".sub-title{text-align:center; color:#718096; margin-bottom:30px; font-size:14px; font-weight:400;}"
                      ".f-group{margin-bottom:20px;}"
                      "label{font-weight:600; color:#4a5568; display:block; margin-bottom:8px; font-size:14px;}"
                      "input,select{width:100%; padding:14px; border:1px solid #cbd5e0; border-radius:12px; box-sizing:border-box; text-align:center; font-size:16px; font-family:'Cairo', sans-serif; background-color:#f8fafc; color:#2d3748; transition: all 0.3s ease; font-weight:600;}"
                      "input:focus, select:focus{outline:none; border-color:#3182ce; background-color:#fff; box-shadow: 0 0 0 3px rgba(66, 153, 225, 0.15);}"
                      "button{background: linear-gradient(135deg, #2b6cb0, #1a365d); color:white; border:none; padding:16px; border-radius:12px; width:100%; font-size:16px; font-weight:700; font-family:'Cairo', sans-serif; cursor:pointer; margin-top:15px; box-shadow:0 4px 12px rgba(26, 54, 93, 0.2); transition: all 0.3s ease;}"
                      "button:hover{background: linear-gradient(135deg, #1a365d, #2b6cb0); transform: translateY(-1px); box-shadow:0 6px 20px rgba(26, 54, 93, 0.3);}"
                      ".footer{margin-top:30px; font-size:12px; color:#a0aec0; text-align:center; font-weight:600;}"
                      "</style></head><body>"
                      "<div class='yt-banner'>"
                      "<a href='https://www.youtube.com/@DarbatShakoush' target='_blank'>"
                      "<img src='https://cdn-icons-png.flaticon.com/512/1384/1384060.png' alt='قناة اليوتيوب'>"
                      "<div> تابعنا على اليوتيوب واشترك الآن</div></a>"
                      "</div>"
                      "<div class='card'><h2>🧮 حاسبة المقاسات والبضاعة الذكية</h2>"
                      "<div class='sub-title'>النظام الهندسي المطور لتصفية وحساب بضاعة المصاعد فوراً</div>"
                      "<form action='/calculate' method='get'>"
                      "<div class='f-group'><label>📦 نوع نظام الهندسة:</label><select name='m_type'><option value='MR'>غرفة محرك أعلى البئر (MR)</option><option value='MRL'>بدون غرفة محرك (MRL)</option></select></div>"
                      "<div class='f-group'><label>📏 عرض البئر الحُر (CM):</label><input type='number' name='width' required min='80' max='250' placeholder='أدخل عرض البئر بالسم'></div>"
                      "<div class='f-group'><label>📐 عمق البئر الحُر (CM):</label><input type='number' name='depth' required min='80' max='250' placeholder='أدخل عمق البئر بالسم'></div>"
                      "<div class='f-group'><label>🏢 عدد أدوار المبنى (الوقفات):</label><input type='number' name='floors' required min='1' max='60' placeholder='أدخل إجمالي الأدوار'></div>"
                      "<button type='submit'>🚀 تحليل الأبعاد وتصفية المقايسة</button></form></div>"
                      "<div class='footer'>انشاء وتطوير: محمد الشعراوي </div>"
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 2️⃣ صفحة تقرير المقايسة الحالية (شغلك الأصلي بالملي)
    svr.Get("/calculate", [&elevator](const httplib::Request& req, httplib::Response& res) {
        string m_type = html_escape(req.get_param_value("m_type"));
        if (m_type != "MR" && m_type != "MRL") { m_type = "MR"; }

        int w = safe_stoi(req.get_param_value("width"), 0);
        int d = safe_stoi(req.get_param_value("depth"), 0);
        float f = safe_stof(req.get_param_value("floors"), 0.0f);

        w = clamp_int(w, 80, 250);
        d = clamp_int(d, 80, 250);
        f = clamp_float(f, 1.0f, 60.0f);

        string door = elevator.get_door_type(w);
        int cabin_dbg = elevator.get_cabin_dbg(w);
        int cwt_dbg = elevator.get_cwt_dbg(w);
        int cab_w = elevator.get_cabin_width(w);
        int cab_d = elevator.get_cabin_depth(d);
        float h = elevator.get_shaft_height(f, m_type);

        int brackets = elevator.calc_brackets(h);
        int bolts = elevator.calc_bolts(brackets);
        float ropes = elevator.calc_ropes(h);
        int fishplates = ((int)f) * 4;
        float rail_qty = (h * 4) / 5.0f; 

        float c_brackets = brackets * elevator.get_p_bracket();
        float c_bolts = bolts * elevator.get_p_bold();
        float c_ropes = ropes * elevator.get_p_rope();
        float c_fishplates = fishplates * elevator.get_p_fish();
        float c_rail = rail_qty * elevator.get_p_rail();
        
        float total = c_brackets + c_bolts + c_ropes + c_fishplates + c_rail;

        ostringstream os;
        os << "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
           << "<link href='https://fonts.googleapis
