#include "httplib.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <algorithm>

using namespace std;

// دوال التحويل والحماية
static int safe_stoi(const string& s, int default_val = 0) {
    try { return s.empty() ? default_val : stoi(s); } catch (...) { return default_val; }
}

static float safe_stof(const string& s, float default_val = 0.0f) {
    try { return s.empty() ? default_val : stof(s); } catch (...) { return default_val; }
}

static int clamp_int(int v, int lo, int hi) { return max(lo, min(hi, v)); }
static float clamp_float(float v, float lo, float hi) { return max(lo, min(hi, v)); }

static string html_escape(const string& data) {
    string buffer;
    buffer.reserve(data.size());
    for (size_t pos = 0; pos != data.size(); ++pos) {
        switch (data[pos]) {
            case '&':  buffer.append("&amp;"); break;
            case '\"': buffer.append("&quot;"); break;
            case '\'': buffer.append("&apos;"); break;
            case '<':  buffer.append("&lt;");   break;
            case '>':  buffer.append("&gt;");   break;
            default:   buffer.push_back(data[pos]); break;
        }
    }
    return buffer;
}

class Elevator {
public:
    string get_door_type(int sa) {
        if (sa >= 191 && sa <= 210) return "Auto 90 CO";
        if (sa > 168 && sa <= 190) return "Auto 80 CO";
        if (sa >= 157 && sa <= 168) return "Auto 70 CO";
        if (sa >= 175 && sa <= 200) return "Auto 100 SI";
        if (sa >= 158 && sa <= 175) return "Auto 90 SI";
        if (sa >= 144 && sa <= 160) return "Auto 80 SI";
        if (sa >= 128 && sa < 144) return "Auto 70 SI";
        if (sa >= 121 && sa <= 135) return "Semi Auto 80";
        if (sa >= 105 && sa <= 120) return "Semi Auto 70";
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
    float get_shaft_height(float f, string t) { return (t == "MRL") ? (f * 4) + 1.5f : (f * 4.0f); }
    int calc_brackets(float h) { return (int)((h / 2.0f) * 4); }
    int calc_bolts(int b) { return b * 4; }
    float calc_ropes(float h) { return ((h * 2) + 5) * 4; }
};

int main() {
    httplib::Server svr;
    Elevator elevator;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        string html = "<html><head><meta charset='UTF-8'><title>حاسبة ضربة شاكوش</title></head><body>"
                      "<div style='text-align:center; padding:50px;'><h1>مرحباً بك في حاسبة المصاعد</h1>"
                      "<a href='/calculate_page' style='font-size:20px;'>ابدأ المقايسة الآن</a></div></body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // صفحة الإدخال
    svr.Get("/calculate_page", [](const httplib::Request&, httplib::Response& res) {
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;700&display=swap' rel='stylesheet'>"
                      "<style>body{font-family:'Cairo'; padding:20px; text-align:right; direction:rtl;}</style></head>"
                      "<body><h2>حاسبة المقاسات</h2><form action='/calculate' method='get'>"
                      "<label>نوع النظام:</label><select name='m_type'><option value='MR'>MR</option><option value='MRL'>MRL</option></select><br>"
                      "<label>عرض البئر:</label><input type='number' name='width' required><br>"
                      "<label>عمق البئر:</label><input type='number' name='depth' required><br>"
                      "<label>عدد الأدوار:</label><input type='number' name='floors' required><br>"
                      "<button type='submit'>تحليل</button></form></body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // صفحة النتائج مع التنسيق الاحترافي
    svr.Get("/calculate", [&elevator](const httplib::Request& req, httplib::Response& res) {
        string m_type = (req.get_param_value("m_type") == "MRL") ? "MRL" : "MR";
        int w = clamp_int(safe_stoi(req.get_param_value("width")), 80, 250);
        int d = clamp_int(safe_stoi(req.get_param_value("depth")), 80, 250);
        float f = clamp_float(safe_stof(req.get_param_value("floors")), 1.0f, 60.0f);

        float h = elevator.get_shaft_height(f, m_type);
        int brackets = elevator.calc_brackets(h);

        ostringstream os;
        os << "<html><head><meta charset='UTF-8'><style>"
           << "body{font-family:'Cairo', sans-serif; direction:rtl; background:#f4f7fc; padding:20px;}"
           << ".box{background:white; padding:40px; border-radius:20px; max-width:600px; margin:auto; border:1px solid #ddd;}"
           << "@media print {"
           << " .btn-print, .btn-back { display:none !important; }"
           << " .box::before { content:'شركة ضربة شاكوش للمصاعد - تقرير هندسي معتمد'; display:block; text-align:center; font-weight:bold; margin-bottom:20px; border-bottom:2px solid #333; padding-bottom:10px; }"
           << " .box::after { content:'تم استخراج التقرير من darbat-shakosh.com'; display:block; text-align:center; margin-top:50px; font-size:10px; }"
           << "}"
           << "</style></head><body><div class='box'>"
           << "<h2>تقرير تصفية المقايسة</h2>"
           << "<p>إجمالي مشوار البئر: " << h << " متر</p>"
           << "<p>عدد الكوابيل: " << brackets << " قطعة</p>"
           << "<button class='btn-print' onclick='window.print()'>🖨️ طباعة التقرير</button>"
           << "<a class='btn-back' href='/calculate_page'>🔄 عودة</a>"
           << "</div></body></html>";
        res.set_content(os.str(), "text/html; charset=utf-8");
    });

    const char* port_env = getenv("PORT");
    svr.listen("0.0.0.0", port_env ? stoi(port_env) : 8080);
    return 0;
}
