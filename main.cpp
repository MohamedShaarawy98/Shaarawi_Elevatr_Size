#include "httplib.h" 
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>

using namespace std;

class Elevator {
private:
    // يمكنك تعديل الأسعار الحقيقية للسوق هنا مباشرة
    const float P_BRACKET = 150.0;  
    const float P_BOLT = 25.0;      
    const float P_ROPE = 80.0;
    const float P_FISH = 45.0;

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
};

int main() {
    httplib::Server svr;
    Elevator elevator;

    // الصفحة الرئيسية للحاسبة العامة
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><style>"
                      "body{background:#f0f2f5;font-family:sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;padding:20px;box-sizing:border-box;flex-direction:column;}"
                      ".yt-banner{text-align:center;margin-bottom:15px;}"
                      ".yt-banner img{width:80px;height:80px;border-radius:50%;box-shadow:0 2px 10px rgba(0,0,0,0.1);border:3px solid #ff0000;}"
                      ".yt-banner a{display:block;color:#ff0000;text-decoration:none;font-weight:bold;margin-top:5px;font-size:16px;}"
                      ".card{background:white;padding:35px;border-radius:16px;box-shadow:0 6px 20px rgba(0,0,0,0.1);width:90%;max-width:600px;direction:rtl;text-align:right;box-sizing:border-box;}"
                      "h2{color:#28a745;text-align:center;margin-bottom:25px;font-size:24px;}.f-group{margin-bottom:15px;}"
                      "label{font-weight:600;color:#495057;display:block;margin-bottom:6px;font-size:16px;}"
                      "input,select{width:100%;padding:12px;border:1px solid #ced4da;border-radius:8px;box-sizing:border-box;text-align:center;font-size:18px;background:#f8f9fa;}"
                      "button{background:#ff0000;color:white;border:none;padding:15px;border-radius:8px;width:100%;font-size:18px;font-weight:bold;cursor:pointer;margin-top:20px;box-shadow:0 4px 10px rgba(255,0,0,0.2);}"
                      "button:hover{background:#cc0000;}"
                      ".footer{margin-top:20px;font-size:14px;color:#6c757d;text-align:center;}"
                      "</style></head><body>"
                      
                      // هنا لوجو القناة والروابط بتاعتك يا فنان
                      "<div class='yt-banner'>"
                      "<a href='https://www.youtube.com/@YOUR_CHANNEL' target='_blank'>"
                      "<img src='https://cdn-icons-png.flaticon.com/512/1384/1384060.png' alt='قناة اليوتيوب'>" // استبدل الرابط ده برابط صورتك الشخصية أو لوجو قناتك
                      "<div>🔴 تابعنا على اليوتيوب واشترك الآن</div></a>"
                      "</div>"
                      
                      "<div class='card'><h2>🧮 حاسبة مقاسات وبضاعة المصاعد الحرة</h2>"
                      "<p style='text-align:center;color:#6c757d;margin-top:-15px;'>أدخل الأبعاد واحسب تكملة البضاعة فوراً</p>"
                      "<form action='/calculate' method='get'>"
                      "<div class='f-group'><label>نوع النظام:</label><select name='m_type'><option value='MR'>بغرفة محرك (MR)</option><option value='MRL'>بدون غرفة محرك (MRL)</option></select></div>"
                      "<div class='f-group'><label>1. عرض البئر (CM):</label><input type='number' name='width' required placeholder='مثال: 160'></div>"
                      "<div class='f-group'><label>2. عمق البئر (CM):</label><input type='number' name='depth' required placeholder='مثال: 160'></div>"
                      "<div class='f-group'><label>3. عدد الأدوار:</label><input type='number' name='floors' required placeholder='مثال: 5'></div>"
                      "<button type='submit'>🚀 تصفية الحسابات وال بضاعة</button></form></div>"
                      "<div class='footer'>تم التطوير بكل ❤️ لخدمة فنيين ومصممي المصاعد</div>"
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // صفحة الحساب الفورية (بدون Database)
    svr.Get("/calculate", [&elevator](const httplib::Request& req, httplib::Response& res) {
        string m_type = req.get_param_value("m_type");
        int w = req.get_param_value("width").empty() ? 0 : stoi(req.get_param_value("width"));
        int d = req.get_param_value("depth").empty() ? 0 : stoi(req.get_param_value("depth"));
        float f = req.get_param_value("floors").empty() ? 0.0 : stof(req.get_param_value("floors"));

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

        float c_brackets = brackets * elevator.get_p_bracket();
        float c_bolts = bolts * elevator.get_p_bolt();
        float c_ropes = ropes * elevator.get_p_rope();
        float c_fishplates = fishplates * elevator.get_p_fish();
        float total = c_brackets + c_bolts + c_ropes + c_fishplates;

        ostringstream os;
        os << "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><style>"
           << "body{background:#f0f2f5;font-family:sans-serif;padding:20px;direction:rtl;text-align:right;}"
           << ".box{max-width:600px;margin:auto;background:white;padding:25px;border-radius:12px;box-shadow:0 4px 15px rgba(0,0,0,0.08);}"
           << "h2{color:#ff0000;text-align:center;}h3{color:#495057;border-bottom:2px solid #dee2e6;padding-bottom:4px;}"
           << ".tbl{width:100%;border-collapse:collapse;margin-top:10px;direction:ltr;text-align:left;}"
           << ".tbl th{background:#f8f9fa;padding:8px;border-bottom:1px solid #dee2e6;width:35%;}"
           << ".tbl td{padding:8px;border-bottom:1px solid #dee2e6;}"
           << ".btbl{width:100%;border-collapse:collapse;margin-top:10px;text-align:center;}"
           << ".btbl th{background:#343a40;color:white;padding:8px;}.btbl td{padding:8px;border-bottom:1px solid #dee2e6;}"
           << ".inv{background:#e2f0d9;padding:15px;border-radius:8px;border:2px dashed #385723;margin-top:15px;text-align:center;font-size:18px;font-weight:bold;color:#385723;}"
           << ".table-container{width:100%; overflow-x:auto;}"
           << ".actions{display:flex; justify-content:space-between; margin-top:20px;}"
           << ".btn-print{background:#28a745; color:white; padding:10px 20px; border:none; border-radius:6px; font-weight:bold; cursor:pointer; font-size:16px;}"
           << ".btn-back{background:#007bff; color:white; padding:10px 20px; text-decoration:none; border-radius:6px; font-weight:bold; font-size:16px;}"
           << "@media print{.btn-print, .btn-back {display:none;}}" // إخفاء الأزرار أثناء الطباعة
           << "</style></head><body><div class='box'><h2>📋 تقرير المقايسة التقديرية للبضاعة</h2>"
           << "<h3>📐 أولاً: الأبعاد الهندسية الناتجة</h3>"
           << "<div class='table-container'><table class='tbl'>"
           << "<tr><th>* Door Type:</th><td>" << door << "</td></tr>"
           << "<tr><th>* Cabin DBG:</th><td><b>" << cabin_dbg << " CM</b></td></tr>"
           << "<tr><th>* CWT DBG:</th><td><b>" << (cwt_dbg == 0 ? "Review Official" : to_string(cwt_dbg) + " CM") << "</b></td></tr>"
           << "<tr><th>* Cabin Width:</th><td><b>" << cab_w << " CM</b></td></tr>"
           << "<tr><th>* Cabin Depth:</th><td><b>" << cab_d << " CM</b></td></tr>"
           << "<tr><th>* Shaft Height:</th><td style='color:#fd7e14;font-weight:bold;'>" << h << " Meters</td></tr>"
           << "</table></div>"
           << "<h3>📦 ثانياً: كمية البضاعة المحسوبة</h3>"
           << "<div class='table-container'><table class='btbl'><thead><tr><th>اسم الصنف</th><th>الكمية المطلوبة</th><th>التكلفة التقريبية</th></tr></thead><tbody>"
           << "<tr><td>كوابيل السكك</td><td>" << brackets << " 🛑</td><td>" << c_brackets << " EGP</td></tr>"
           << "<tr><td>مسامير التثبيت</td><td>" << bolts << " 🔩</td><td>" << c_bolts << " EGP</td></tr>"
           << "<tr><td>حبال الواير</td><td>" << ropes << " 🧵</td><td>" << c_ropes << " EGP</td></tr>"
           << "<tr><td>لقم السكك</td><td>" << fishplates << " 🗜️</td><td>" << c_fishplates << " EGP</td></tr>"
           << "</tbody></table></div>"
           << "<div class='inv'>💰 إجمالي التكلفة التقريبية: " << total << " EGP</div>"
           << "<div class='actions'>"
           << "<button class='btn-print' onclick='window.print()'>🖨️ طباعة المقايسة أو حفظ PDF</button>"
           << "<a class='btn-back' href='/'>🔄 حساب مقاس آخر</a>"
           << "</div></div></body></html>";
        res.set_content(os.str(), "text/html; charset=utf-8");
    });

    const char* port_env = getenv("PORT");
    int port = port_env ? stoi(port_env) : 8080;
    svr.listen("0.0.0.0", port);
    return 0;
}
