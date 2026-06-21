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

// هيكل بيانات لتتبع طلبات كل مستخدم (IP)
struct RateLimitInfo {
    int count = 0;
    chrono::steady_clock::time_point reset_time;
};

// متغيرات التحكم في نظام تحديد الطلبات (Rate Limiting)
static map<string, RateLimitInfo> ip_tracker;
static mutex rate_limit_mtx;
const int MAX_REQUESTS_PER_MINUTE = 12; // الحد الأقصى للطلبات في الدقيقة

// ============================================================
//  دوال تحويل وحماية آمنة
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

// دالة ترويسات الأمان المحدثة (مضاف إليها إخفاء هوية السيرفر الأصلي)
static void set_security_headers(httplib::Response& res) {
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-XSS-Protection", "1; mode=block");
    res.set_header("Content-Security-Policy", "default-src 'self' https://fonts.googleapis.com https://fonts.gstatic.com; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; font-src https://fonts.gstatic.com;");
    res.set_header("Server", "Hammer-Engine/1.0"); // التمويه وإخفاء اسم المكتبة البرمجية القياسية
}

// دالة فحص وتطبيق نظام الـ Rate Limiting
static bool is_rate_limited(const string& ip) {
    lock_guard<mutex> lock(rate_limit_mtx);
    auto now = chrono::steady_clock::now();
    
    // إذا كان الـ IP جديداً تماماً أو انتهت دقيقة التتبع السابقة، أعد تعيين العداد
    if (ip_tracker.find(ip) == ip_tracker.end() || now >= ip_tracker[ip].reset_time) {
        ip_tracker[ip].count = 1;
        ip_tracker[ip].reset_time = now + chrono::minutes(1);
        return false;
    }
    
    // زيادة عدد الطلبات
    ip_tracker[ip].count++;
    
    // إذا تخطى الـ 12 طلب في نفس الدقيقة، يتم حظره
    if (ip_tracker[ip].count > MAX_REQUESTS_PER_MINUTE) {
        return true;
    }
    
    return false;
}

// واجهة مخصصة تظهر عند تخطي حد الطلبات المسموح
static void send_rate_limit_error(httplib::Response& res) {
    ostringstream os;
    os << "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
       << "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700&display=swap' rel='stylesheet'>"
       << "<style>"
       << "body{background-color:#121212; font-family:'Cairo', sans-serif; display:flex; align-items:center; justify-content:center; min-height:100vh; margin:0; direction:rtl;}"
       << ".limit-card{background:#1e1e1e; border:2px solid #ecc94b; padding:40px; border-radius:15px; text-align:center; max-width:500px; width:90%; box-shadow:0 10px 25px rgba(236,201,75,0.15);}"
       << ".limit-card h2{color:#ecc94b; font-size:22px; margin-top:0;}"
       << ".limit-card p{color:#ccc; font-size:15px; line-height:1.6; margin-bottom:25px;}"
       << "</style></head><body>"
       << "<div class='limit-card'>"
       << "<h2>⚠️ تم تجاوز حد الطلبات المسموح به</h2>"
       << "<p>لقد قمت بإرسال عدد كبير من الطلبات في وقت قصير (الحد الأقصى هو 12 طلباً في الدقيقة).<br>يرجى الانتظار دقيقة واحدة ثم إعادة المحاولة بشكل طبيعي.</p>"
       << "</div></body></html>";
    res.status = 429; // كود الحالة القياسي لتخطي حد الطلبات (Too Many Requests)
    res.set_content(os.str(), "text/html; charset=utf-8");
}

// ============================================================
//  كلاس المصعد هندسياً
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
        if (v >= 100 && v <= 110) return 72;
        if (v > 110 && v <= 120) return 82;
        if (v > 120 && v <= 125) return 92;
        if (v > 125 && v <= 210) return 102;
        return 0;
    }
    int get_cabin_width(int cw)
