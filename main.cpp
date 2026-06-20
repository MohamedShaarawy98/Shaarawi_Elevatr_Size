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
//  كلاس المصعد: كودك الأصلي القديم بالملي لسهولة التعديل
// ============================================================
class Elevator {
private:
    const float P_BRACKET = 0.0;
    const float P_BOLT = 0.0;
    const float P_ROPE = 0.0;
    const float P_FISH = 0.0;
    const float P_RAIL = 0.0; // سعر قضيب الريل الواحد (طول 5 متر)

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
//  الدالة الرئيسية وتشغيل السيرفر
// ============================================================
int main() {
    httplib::Server svr;
    Elevator elevator;

    // 1️⃣ الصفحة الرئيسية الجديدة (بوابة المنصة بالأزرار الكبيرة)
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
                      "<div class='footer'>إنشاء وتطوير: محمد الشعراوي</div>"
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 2️⃣ صفحة واجهة إدخال بيانات الحاسبة (تم نقلها للمسار /calculator لفتحها من البوابة)
    svr.Get("/calculator", [](const httplib::Request&, httplib::Response& res) {
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
                      "<style>"
                      "body{background-color:##3c85b5; font-family:'Cairo', sans-serif; display:flex; align-items:center; justify-content:center; min-height:100vh; margin:0; padding:20px; box-sizing:border-box; flex-direction:column; color:#2d3748;}"
                      ".card{background: #ffffff; padding:40px; border-radius:20px; box-shadow: 0 10px 30px rgba(160, 174, 192, 0.2); width:95%; max-width:550px; direction:rtl; text-align:right; box-sizing:border-box; border: 1px solid rgba(226, 232, 240, 0.8);}"
                      "h2{color:#1a365d; text-align:center; margin-top:0; margin-bottom:10px; font-weight:700; font-size:24px;}"
                      ".sub-title{text-align:center; color:#718096; margin-bottom:30px; font-size:14px; font-weight:400;}"
                      ".f-group{margin-bottom:20px;}"
                      "label{font-weight:600; color:#4a5568; display:block; margin-bottom:8px; font-size:14px;}"
                      "input,select{width:100%; padding:14px; border:1px solid #cbd5e0; border-radius:12px; box-sizing:border-box; text-align:center; font-size:16px; font-family:'Cairo', sans-serif; background-color:#f8fafc; color:#2d3748; transition: all 0.3s ease; font-weight:600;}"
                      "input:focus, select:focus{outline:none; border-color:#3182ce; background-color:#fff; box-shadow: 0 0 0 3px rgba(66, 153, 225, 0.15);}"
                      "button{background: linear-gradient(135deg, #2b6cb0, #1a365d); color:white; border:none; padding:16px; border-radius:12px; width:100%; font-size:16px; font-weight:700; font-family:'Cairo', sans-serif; cursor:pointer; margin-top:15px; box-shadow:0 4px 12px rgba(26, 54, 93, 0.2); transition: all 0.3s ease;}"
                      "button:hover{background: linear-gradient(135deg, #1a365d, #2b6cb0); transform: translateY(-1px); box-shadow:0 6px 20px rgba(26, 54, 93, 0.3);}"
                      ".btn-home{display:block; text-align:center; margin-top:20px; color:#3182ce; text-decoration:none; font-weight:700; font-size:14px;}"
                      "</style></head><body>"
                      "<div class='card'><h1>🧮 حاسبة المقاسات والبضاعة الذكية</h1>"
                      "<div class='sub-title'>النظام الهندسي المطور لتصفية وحساب بضاعة المصاعد فوراً</div>"
                      "<form action='/calculate' method='get'>"
                      "<div class='f-group'><label>📦 نوع نظام الهندسة:</label><select name='m_type'><option value='MR'>غرفة محرك أعلى البئر (MR)</option><option value='MRL'>بدون غرفة محرك (MRL)</option></select></div>"
                      "<div class='f-group'><label>📏 عرض البئر الحُر (CM):</label><input type='number' name='width' required min='80' max='250' placeholder='أدخل عرض البئر بالسنتيمتر'></div>"
                      "<div class='f-group'><label>📐 عمق البئر الحُر (CM):</label><input type='number' name='depth' required min='80' max='250' placeholder='أدخل عمق البئر بالسنتيمتر'></div>"
                      "<div class='f-group'><label>🏢 عدد أدوار المبنى (الوقفات):</label><input type='number' name='floors' required min='1' max='60' placeholder='أدخل إجمالي الأدوار'></div>"
                      "<button type='submit'>🚀 تحليل الأبعاد وتصفية المقايسة</button></form>"
                      "<a href='/' class='btn-home'>⬅️ العودة للبوابة الرئيسية</a></div>"
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 3️⃣ صفحة تقرير المقايسة (شغلك وحساباتك الأصلية بالملي)
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
        float c_bolts = bolts * elevator.get_p_bolt(); 
        float c_ropes = ropes * elevator.get_p_rope();
        float c_fishplates = fishplates * elevator.get_p_fish();
        float c_rail = rail_qty * elevator.get_p_rail();
        
        float total = c_brackets + c_bolts + c_ropes + c_fishplates + c_rail;

        ostringstream os;
        os << "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
           << "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
           << "<style>"
           << "body{background-color:#1a5d3e; font-family:'Cairo', sans-serif; padding:30px 10px; direction:rtl; text-align:right; color:#2d3748;}"
           << ".box{max-width:650px; margin:auto; background:#ffffff; padding:35px; border-radius:20px; box-shadow:0 10px 30px rgba(160,174,192,0.15); border: 1px solid #e2e8f0;}"
           << "h2{color:#1a365d; text-align:center; margin-top:0; font-weight:700; font-size:22px; border-bottom:2px solid #e2e8f0; padding-bottom:15px;}"
           << "h3{color:#2b6cb0; font-size:15px; font-weight:700; margin-top:25px; margin-bottom:12px; display:flex; align-items:center;}"
           << ".table-container{width:100%; overflow-x:auto; background:#fff; border-radius:12px; border:1px solid #edf2f7; margin-top:8px;}"
           << ".tbl{width:100%; border-collapse:collapse; text-align:right;}"
           << ".tbl th{background:#f7fafc; padding:12px 15px; color:#4a5568; font-weight:600; border-bottom:1px solid #edf2f7; font-size:14px; width:45%;}"
           << ".tbl td{padding:12px 15px; border-bottom:1px solid #edf2f7; color:#1a202c; font-size:14px; font-weight:600;}"
           << ".btbl{width:100%; border-collapse:collapse; text-align:center;}"
           << ".btbl th{background:#2d3748; color:white; padding:12px; font-weight:600; font-size:13px;}"
           << ".btbl td{padding:12px; border-bottom:1px solid #edf2f7; color:#2d3748; font-size:14px; font-weight:600;}"
           << ".btbl tr:nth-child(even){background-color: #f8fafc;}"
           << ".inv{background:linear-gradient(135deg, #f0fff4, #c6f6d5); padding:18px; border-radius:12px; border:1px dashed #38a169; margin-top:25px; text-align:center; font-size:18px; font-weight:700; color:#22543d; box-shadow: 0 4px 6px rgba(56,161,105,0.05);}"
           << ".actions{display:flex; justify-content:space-between; margin-top:30px; gap:15px;}"
           << ".btn-print{background:#2f855a; color:white; padding:12px 25px; border:none; border-radius:10px; font-weight:700; font-family:'Cairo', sans-serif; cursor:pointer; font-size:14px; flex:1; box-shadow:0 4px 10px rgba(47,133,90,0.2); transition:all 0.3s;}"
           << ".btn-print:hover{background:#22543d; transform:translateY(-1px);}"
           << ".btn-back{background:#3182ce; color:white; padding:12px 25px; text-decoration:none; border-radius:10px; font-weight:700; font-size:14px; text-align:center; flex:1; box-shadow:0 4px 10px rgba(49,130,206,0.2); transition:all 0.3s;}"
           << ".btn-back:hover{background:#2b6cb0; transform:translateY(-1px);}"
           << "@media print{.btn-print, .btn-back, h2 {display:none;} .box{box-shadow:none; padding:0; border:none;}}"
           << "</style></head><body><div class='box'><h2>📋 تقرير تصفية الأبعاد الفنية والمقايسة</h2>"
           << "<h3>📐 أولاً: البيانات الهندسة الناتجة</h3>"
           << "<div class='table-container'><table class='tbl'>"
           << "<tr><th>نوع الباب الافتراضي:</th><td style='color:#3182ce;'>" << door << "</td></tr>"
           << "<tr><th>مقاس DBG الكابينة:</th><td>" << cabin_dbg << " CM</td></tr>"
           << "<tr><th>مقاس DBG الثقل (CWT):</th><td>" << (cwt_dbg == 0 ? "مراجعة يدوي للمقاس" : to_string(cwt_dbg) + " CM") << "</td></tr>"
           << "<tr><th>صافي عرض الكابينة الداخلي:</th><td>" << cab_w << " CM</td></tr>"
           << "<tr><th>صافي عمق الكابينة الداخلي:</th><td>" << cab_d << " CM</td></tr>"
           << "<tr><th>إجمالي مشوار البئر المحسوب:</th><td style='color:#dd6b20;'>" << h << " متر</td></tr>"
           << "</table></div>"
           << "<h3>📦 ثانياً: كمية البضاعة المحسوبة للمشوار</h3>"
           << "<div class='table-container'><table class='btbl'><thead><tr><th>اسم الصنف ومواصفاته</th><th>الكمية</th><th>التكلفة التقديرية</th></tr></thead><tbody>"
           << "<tr><td>كوابيل السكك الحديدية</td><td>" << brackets << " قطعة 🛑</td><td>" << c_brackets << " EGP</td></tr>"
           << "<tr><td>مسامير وجوايط التثبيت</td><td>" << bolts << " مسمار 🔩</td><td>" << c_bolts << " EGP</td></tr>"
           << "<tr><td>حبال واير الفولاذ</td><td>" << ropes << " متر 🧵</td><td>" << c_ropes << " EGP</td></tr>"
           << "<tr><td>لقم ربط السكك (التقفيل)</td><td>" << fishplates << " لقمة 🗜️</td><td>" << c_fishplates << " EGP</td></tr>"
           << "<tr><td>قضبان السكك الحديدية (الريل)</td><td>" << rail_qty << " قضيب (5م) 🛤️</td><td>" << c_rail << " EGP</td></tr>"
           << "</tbody></table></div>"
           << "<div class='inv'>💰 إجمالي القيمة المالية التقديرية: " << total << " EGP</div>"
           << "<div class='actions'>"
           << "<button class='btn-print' onclick='window.print()'>🖨️ طباعة التقرير / حفظ PDF</button>"
           << "<a class='btn-back' href='/calculator'>🔄 حساب مقايسة جديدة</a>"
           << "</div></div></body></html>";
        res.set_content(os.str(), "text/html; charset=utf-8");
    });

    // 4️⃣ مسار المقالات المستقبلي الجديد
    svr.Get("/blog", [](const httplib::Request&, httplib::Response& res) {
        string blog_html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                           "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
                           "<style>"
                           "body{background-color:#121212; font-family:'Cairo', sans-serif; color:#fff; direction:rtl; padding:40px 20px;}"
                           ".container{max-width:800px; margin:auto;}"
                           "h1{color:#ffcc00; border-bottom:2px solid #ffcc00; padding-bottom:10px;}"
                           ".card{background:#1e1e1e; padding:20px; border-radius:10px; margin-top:20px; border:1px solid #333;}"
                           "h2{color:#ffcc00; font-size:20px;}"
                           "p{color:#aaa; line-height:1.6;}"
                           ".btn-back{display:inline-block; margin-top:20px; background:#3182ce; color:white; padding:10px 20px; text-decoration:none; border-radius:50px; font-weight:700;}"
                           "</style></head><body><div class='container'>"
                           "<h1>📚 بوابة ضربة شاكوش للمقالات والشروحات الهندسية</h1>"
                           "<div class='card'><h2>قريباً: شرح مخططات DWG للمصاعد</h2><p>هنا سيتم رفع الشروحات الفنية المفصلة لتركيب السكك والمقاسات القياسية لكوابين المصاعد هيدروليك وجيرلس...</p></div>"
                           "<div class='card'><h2>قريباً: التحكم البرمجي بالـ C++ وكروت الروبوتات</h2><p>شرح عملي لكيفية تحويل الأوامر الحسابية إلى إشارات ميكانيكية دقيقة للـ CNC ومحركات التوجيه...</p></div>"
                           "<a class='btn-back' href='/'>🧮 العودة للبوابة الرئيسية</a>"
                           "</div></body></html>";
        res.set_content(blog_html, "text/html; charset=utf-8");
    });

    // تشغيل السيرفر على بورت ريندر
    const char* port_env = getenv("PORT");
    int port = port_env ? safe_stoi(port_env, 8080) : 8080;
    svr.listen("0.0.0.0", port);
    return 0;
}
