#include "httplib.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <algorithm>

using namespace std;

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
    const float P_RAIL = 1200.0;

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
    float get_p_bolt() { return P_BOLT; }
    float get_p_rope() { return P_ROPE; }
    float get_p_fish() { return P_FISH; }
    float get_p_rail() { return P_RAIL; }
};

// ============================================================
//  الدالة الرئيسية وتشغيل السيرفر بالتقسيم الشامل الجديد
// ============================================================
int main() {
    httplib::Server svr;
    Elevator elevator;

    // 1️⃣ المسار الرئيسي (البوابة الشاملة الفخمة للأزرار والتقسيمات)
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
                      "<style>"
                      "body{background-color:#121212; font-family:'Cairo', sans-serif; color:#fff; direction:rtl; padding:20px; display:flex; flex-direction:column; align-items:center; min-height:100vh; margin:0;}"
                      "header{text-align:center; margin:40px 0;}"
                      "header h1{color:#ffcc00; font-size:2.5rem; margin-bottom:10px;}"
                      "header p{color:#aaa; font-size:1rem;}"
                      ".grid-nav{display:grid; grid-template-columns:repeat(auto-fit, minmax(280px, 1fr)); gap:25px; width:100%; max-width:900px; margin-top:20px;}"
                      ".nav-card{background:#1e1e1e; border:1px solid #333; padding:30px; border-radius:15px; text-align:center; text-decoration:none; color:#fff; transition:0.3s; display:flex; flex-direction:column; align-items:center; justify-content:center;}"
                      ".nav-card:hover{border-color:#ffcc00; transform:translateY(-5px); box-shadow:0 10px 20px rgba(255,204,0,0.15);}"
                      ".nav-card h3{color:#ffcc00; font-size:1.4rem; margin-bottom:12px;}"
                      ".nav-card p{color:#aaa; font-size:0.9rem; line-height:1.5;}"
                      ".disabled{opacity:0.5; cursor:not-allowed;}"
                      ".disabled:hover{transform:none; border-color:#333; box-shadow:none;}"
                      ".footer{margin-top:auto; padding:40px 0 20px 0; font-size:13px; color:#555; text-align:center; font-weight:600;}"
                      "</style></head><body>"
                      "<header><h1>ضربة شاكوش 🛠️</h1><p>المنصة الهندسية المعتمدة لتقنيات المصاعد والتحكم البرمجي</p></header>"
                      "<div class='grid-nav'>"
                      "<a href='/calculator' class='nav-card'><h3>🧮 حاسبة مقاسات البضاعة</h3><p>تصفية أبعاد بئر المصعد وحساب الكابينة والمواد هندسياً بأعلى دقة.</p></a>"
                      "<a href='/blog' class='nav-card'><h3>📚 مقالات وشروحات عملي</h3><p>مخططات DWG، طرق صيانة الكروت الإلكترونية، وبرمجة الروبوتات بالـ C++.</p></a>"
                      "<div class='nav-card disabled'><h3>🤖 تحكم الروبوتات والـ CNC</h3><p>(قريباً) واجهة حساب معاملات الحركة ومحاور الـ CNC بالـ C++.</p></div>"
                      "</div>"
                      "<div class='footer'>تطوير وإشراف هندسي: محمد الشعراوي</div>"
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 2️⃣ مسار واجهة الحاسبة الذكية (شغلك الأصلي والجميل)
    svr.Get("/calculator", [](const httplib::Request&, httplib::Response& res) {
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
                      "<style>"
                      "body{background-color:#f4f7fc; font-family:'Cairo', sans-serif; display:flex; align-items:center; justify-content:center; min-height:100vh; margin:0; padding:20px; box-sizing:border-box; flex-direction:column; color:#2d3748;}"
                      ".card{background: #ffffff; padding:40px; border-radius:20px; box-shadow: 0 10px 30px rgba(160, 174, 192, 0.2); width:95%; max-width:550px; direction:rtl; text-align:right; box-sizing:border-box; border: 1px solid rgba(226, 232, 240, 0.8);}"
                      "h2{color:#1a365d; text-align:center; margin-top:0; margin-bottom:10px; font-weight:700; font-size:24px;}"
                      ".sub-title{text-align:center; color:#718096; margin-bottom:30px; font-size:14px; font-weight:400;}"
                      ".f-group{margin-bottom:20px;}"
                      "label{font-weight:600; color:#4a5568; display:block; margin-bottom:8px; font-size:14px;}"
                      "input,select{width:100%; padding:14px; border:1px solid #cbd5e0; border-radius:12px; box-sizing:border-box; text-align:center; font-size:16px; font-family:'Cairo', sans-serif; background-color:#f8fafc; color:#2d3748; transition: all 0.3s ease; font-weight:600;}"
                      "input:focus, select:focus{outline:none; border-color:#3182ce; background-color:#fff; box-shadow: 0 0 0 3px rgba(66, 153, 225, 0.15);}"
                      "button{background: linear-gradient(135deg, #2b6cb0, #1a365d); color:white; border:none; padding:16px; border-radius:12px; width:100%; font-size:16px; font-weight:700; font-family:'Cairo', sans-serif; cursor:pointer; margin-top:15px; box-shadow:0 4px 12px rgba(26, 54, 93, 0.2); transition: all 0.3s ease;}"
                      "button:hover{background: linear-gradient(135deg, #1a365d, #2b6cb0); transform: translateY(-1px); box-shadow:0 6px 20px rgba(26, 54, 93, 0.3);}"
                      ".btn-home{display:block; text-align:center; margin-top:
