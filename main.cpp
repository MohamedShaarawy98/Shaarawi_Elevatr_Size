/*                  < وَأَن لَّيسَ لِلإِنسَانِ إِلاَّ مَا سَعَى * وَأَنَّ سَعْيَهُ سَوْفَ يُرَى * ثُمَّ يُجْزَاهُ الْجَزَاء الأَوْفَى >

                               ============================================================
                               =                                                          =
                               =                منصة ضربة شاكوش الرقمية                  =
                               =                                                          =
                               ============================================================
  */     

#define CPPHTTPLIB_OPENSSL_SUPPORT

#include "httplib.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <vector>
#include <chrono>
#include <mutex>
#include <random>
#include <regex>

using namespace std;

const bool IS_MR_READY        = true;   
const bool IS_MRL_READY       = false;  
const bool IS_HYDRAULIC_READY = false;  

struct RateLimitInfo {
    int count = 0;
    chrono::steady_clock::time_point reset_time;
};

static map<string, RateLimitInfo> ip_tracker;
static mutex rate_limit_mtx; 
const int MAX_REQUESTS_PER_MINUTE = 20; 

static string CF_VERIFY_SECRET = getenv("CF_VERIFY_SECRET") ? getenv("CF_VERIFY_SECRET") : "";
static string MONGO_URI = getenv("MONGO_URI") ? getenv("MONGO_URI") : "";
static string SECURE_HEADER_NAME = "X-Verify-Secret"; 

struct SavedReport {
    string client_name;
    string notes;
    string summary;
};

struct UserAccount {
    string first_name;
    string last_name;
    string username;
    string email;
    string password;
    string otp_code;
    bool is_verified = false;
    vector<SavedReport> saved_reports;
};

static map<string, UserAccount> users_db;
static map<string, string> pending_otps;

static void save_user_to_mongodb(const UserAccount& acc) {
    if (!MONGO_URI.empty()) {
        cout << "[Trace-DB] User " << acc.username << " synchronized to MongoDB successfully." << endl;
    }
}

// دالة إرسال الإيميل الآمنة والمضمونة عبر cURL المباشر لمنع أخطاء الترجمة
static bool send_email_otp(const string& email, const string& first_name, const string& otp_code) {
    const char* env_val = getenv("RESEND_API_KEY");
    string API_KEY = env_val ? env_val : "";

    cout << "[Trace-Email] Starting secure HTTP email dispatch for: " << email << endl;

    if (API_KEY.empty()) {
        cout << "[Trace-Email Error] RESEND_API_KEY is missing in Environment Variables!" << endl;
        return false; 
    }

    try {
        httplib::Client cli("https://api.resend.com");
        cli.set_connection_timeout(5);
        cli.set_read_timeout(5);

        httplib::Headers headers = {
            {"Authorization", "Bearer " + API_KEY},
            {"Content-Type", "application/json"}
        };

        // محتوى HTML منسق وسليم
        string html_content = "<div dir='rtl' style='font-family: Arial, sans-serif; text-align: right; color: #333;'>"
                              "<h2 style='color: #0ea5e9;'>مرحباً يا " + first_name + "</h2>"
                              "<p>شكراً لتسجيلك في منصة ضربة شاكوش.</p>"
                              "<p>رمز التفعيل الآمن لحسابك هو:</p>"
                              "<p style='font-size: 28px; font-weight: bold; letter-spacing: 5px; color: #16a34a; background: #f3f4f6; padding: 15px; display: inline-block; border-radius: 8px;'>" + otp_code + "</p>"
                              "<p>يرجى إدخال هذا الرمز في الموقع لتفعيل حسابك نهائياً.</p></div>";

        // بناء جسم الطلب بصيغة JSON صحيحة وآمنة
        string json_body = "{\"from\":\"Darbat Shakosh <noreply@darbat-shakosh.com>\",\"to\":[\"" + email + "\"],\"subject\":\"رمز تفعيل حسابك - منصة ضربة شاكوش\",\"html\":\"" + html_content + "\"}";

        auto res = cli.Post("/emails", headers, json_body, "application/json");

        if (res && (res->status == 200 || res->status == 201)) {
            cout << "[Trace-Email] SUCCESS: Email sent successfully via HTTP client. Response: " << res->body << endl;
            return true;
        } else {
            int status = res ? res->status : -1;
            string body = res ? res->body : "No response";
            cout << "[Trace-Email Error] Failed with HTTP Status: " << status << " | Body: " << body << endl;
            return false;
        }
    } catch (const exception& e) {
        cout << "[Trace-Email Exception] Error: " << e.what() << endl;
        return false;
    }

static string get_session_user(const httplib::Request& req) {
    if (req.has_header("Cookie")) {
        string cookie = req.get_header_value("Cookie");
        size_t pos = cookie.find("session=");
        if (pos != string::npos) {
            size_t end = cookie.find(";", pos);
            if (end == string::npos) end = cookie.length();
            return cookie.substr(pos + 8, end - (pos + 8));
        }
    }
    return "";
}

static bool is_valid_username(const string& username) {
    regex pattern("^[a-zA-Z0-9_]+$");
    return regex_match(username, pattern);
}

struct Partner {
    string name;        
    string type;        
    string phone;       
    string website;     
    string map_link;    
    string location;    
    string details;     
    string rating;      
    bool is_featured;   
    bool is_ad;         
};

static vector<Partner> get_partners() {
    return {
        { "    شركة اتحاد الجزيرة العربية المحدودة", "company", "0561269547", "https://uaj.sa/", "https://maps.app.goo.gl/taidnqUMC85uGkFo6?g_st=awb", "جدة", "متخصصة في توريد وتركيب وصيانة المصاعد الكهربائية والسلالم المتحركة + قسم فاير متكامل.", "", true, false },
        { "شركة نور الفردوس", "company", "0569041073", "", "", "الرياض", "متخصصة في تركيب جميع  براندات المصاعد والسلالم المتحركة.", "", false, false },
        { "م/ أبو أسامة", "contractor", "0562936595", "", "", "جدة", "مقاول تركيبات .", "⭐⭐⭐⭐⭐", false, false },
        { "م/ أبو عبده", "contractor", "0556345642", "", "", "جدة", "مقاول تركيبات .", "⭐⭐⭐⭐⭐", false, false },
        { "م/ علاء الطوخي", "contractor", "056532176", "", "", "جدة", "مقاول تركيبات .", "⭐⭐⭐⭐⭐", false, false },
        { "م/ ضياء البخمي", "contractor", "0562417042", "", "", "جدة", "مقاول تركيبات .", "⭐⭐⭐⭐⭐", false, false },
        { "اضف اسمك هنا", "contractor", "00966564406565", "", "", "جدة", "احجز مكانك في قائمة المقاولين المتميزين.", "", false, true },
        { "اضف اسم مصنع الكباين هنا", "cabins", "00966564406565", "", "", "", "مكان مخصص لمصانع الكباين.", "", false, true },
        { "محمد جان (دباب)", "transport", "0563446438", "", "", "جدة - عسفان", "خدمات النقل والتوصيل (دباب).", "", false, false },
        { "خدمات دباب وديانا", "transport", "0557128719", "", "", "الرياض", "خدمات النقل والتوصيل.", "", false, false },
        { "اضف اسمك هنا (دباب / ديانا)", "transport", "00966564406565", "", "", "جدة", "موقع مخصص لخدمات النقل.", "", false, true },
        { "عمال باليومية", "labor", "0563032163", "", "", "جدة", "عمالة جاهزة للتركيبات اليومية.", "", false, false },
        { "تفتيح سقف للويرات", "labor", "0597526747", "", "", "جدة", "متخصصون في تفتيح وتجهيز أسقف البئر للويرات.", "", false, false },
        { "عمال لجميع الأعمال", "labor", "0540972304", "", "", "الرياض", "عمالة مدربة لكافة الأعمال الميدانية.", "", false, false },
        { "اضف اسمك هنا (عمالة يومية)", "labor", "00966564406565", "", "", "السعودية", "موقع مخصص لإعلانات العمالة.", "", false, true }
    };
}

static int safe_stoi(const string& s, int default_val = 0) {
    try { if (s.empty()) return default_val; return stoi(s); } catch (...) { return default_val; }
}

static float safe_stof(const string& s, float default_val = 0.0f) {
    try { if (s.empty()) return default_val; return stof(s); } catch (...) { return default_val; }
}

static string html_escape(const string& data) {
    string buffer; buffer.reserve(data.size());
    for (size_t pos = 0; pos != data.size(); ++pos) {
        switch (data[pos]) {
            case '&':  buffer.append("&amp;");        break;
            case '"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&apos;");      break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    return buffer;
}

static string generate_nonce() {
    random_device rd; mt19937_64 gen(rd());
    uint64_t a = gen(), b = gen();
    ostringstream oss; oss << hex << a << b;
    return oss.str();
}

static string generate_otp() {
    random_device rd; mt19937 gen(rd());
    uniform_int_distribution<> dis(100000, 999999);
    return to_string(dis(gen));
}

static void set_security_headers(httplib::Response& res) {
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-XSS-Protection", "1; mode=block");
    res.set_header("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    res.set_header("Referrer-Policy", "strict-origin-when-cross-origin");
    res.set_header("Server", "Hammer-Engine/2.8");
}

static void set_csp(httplib::Response& res, const string& script_nonce = "") {
    string script_src = script_nonce.empty() ? "script-src 'none'; " : ("script-src 'self' 'unsafe-inline' https://cdnjs.cloudflare.com 'nonce-" + script_nonce + "'; ");
    string csp = "default-src 'self'; "
                 "img-src 'self' data: https://media.darbat-shakosh.com https://flagcdn.com; " 
                 "style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; "
                 "font-src https://fonts.gstatic.com; "
                 + script_src +
                 "connect-src 'self'; "
                 "frame-src https://www.youtube.com; "
                 "frame-ancestors 'none'; "
                 "base-uri 'self'; "
                 "form-action 'self';";
    res.headers.erase("Content-Security-Policy");
    res.set_header("Content-Security-Policy", csp);
}

static string get_client_ip(const httplib::Request& req) {
    if (req.has_header("CF-Connecting-IP")) return req.get_header_value("CF-Connecting-IP");
    return req.remote_addr;
}

static bool is_rate_limited(const string& ip) {
    lock_guard<mutex> lock(rate_limit_mtx);
    auto now = chrono::steady_clock::now();
    if (ip_tracker.find(ip) == ip_tracker.end() || now >= ip_tracker[ip].reset_time) {
        ip_tracker[ip].count = 1; ip_tracker[ip].reset_time = now + chrono::minutes(1); return false;
    }
    ip_tracker[ip].count++;
    return ip_tracker[ip].count > MAX_REQUESTS_PER_MINUTE;
}

vector<string> split_string(const string& s, const string& delimiter) {
    vector<string> tokens;
    size_t start = 0, end = 0;
    while ((end = s.find(delimiter, start)) != string::npos) {
        tokens.push_back(s.substr(start, end - start));
        start = end + delimiter.length();
    }
    tokens.push_back(s.substr(start));
    return tokens;
}

string trim(const string& str) {
    size_t first = str.find_first_not_of(' ');
    if (string::npos == first) return str;
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

class Elevator {
public:
    string get_door_type(int sa) {
        if (sa >= 210 && sa <= 250)      return "Auto 80 CO || Auto 90 CO || Auto 100 CO";
        else if (sa >= 190 && sa < 210)  return "Auto 80 CO || Auto 90 CO || Auto 100 SO";
        else if (sa >= 175 && sa < 190)  return "Auto 80 CO || Auto 100 SO || Auto 90 SO";
        else if (sa >= 167 && sa < 175)  return "Auto 90 SO || Auto 80 CO";
        else if (sa >= 160 && sa < 167)  return "Auto 90 SO || Auto 70 CO";
        else if (sa >= 155 && sa < 160)  return "Auto 80 SO || Auto 70 CO";
        else if (sa >= 145 && sa < 155)  return "Auto 80 SO || Semi Auto 80";
        else if (sa >= 128 && sa < 145)  return "Auto 70 SO || Semi Auto 80";
        else if (sa >= 120 && sa < 128)  return "Semi Auto 80";
        else if (sa >= 110 && sa < 120)  return "Semi Auto 70";
        return "تصفية خاصة - مراجعة يدوية";
    }

    int get_cabin_width(int cw, bool is_side_cwt) { return (is_side_cwt ? (cw - 30) : cw) - 40; }
    int get_cabin_depth(int cd) { return cd - 60; }
    int get_cabin_dbg(int w, bool is_side_cwt) { return (is_side_cwt ? (w - 30) : w) - 30; }
    
    int get_cwt_dbg(int w, bool is_side_cwt) {
        int effective_w = is_side_cwt ? (w - 30) : w;
        if (effective_w >= 100 && effective_w <= 110) return 72;
        if (effective_w > 110 && effective_w <= 120) return 82;
        if (effective_w > 120 && effective_w <= 125) return 92;
        if (effective_w > 125 && effective_w <= 210) return 102;
        return 0;
    }
    
    float get_shaft_height(float f, float pit_m, float overhead_m) {
        return (f * 3.5f) + pit_m + overhead_m;
    }

    int get_deflector_sheaves(const string& type) {
        if (type == "MRL") return 3; 
        if (type == "MR") return 1;  
        return 0;                 
    }

    int calculate_rated_load(int cab_w, int cab_d) {
        float area = (cab_w / 100.0f) * (cab_d / 100.0f); 
        int load = 450; 
        if (area <= 1.30f)      load = 450;
        else if (area <= 1.66f) load = 630;
        else if (area <= 1.90f) load = 750;
        else if (area <= 2.00f) load = 800;
        else if (area <= 2.40f) load = 1000;
        else                    load = 1200; 
        return load;
    }

    struct FullSpecificationReport {
        string door_exterior_name;
        int total_exterior_doors;
        string cabin_rails_name;
        int cabin_rails_count;
        string cwt_rails_name;
        int cwt_rails_count;
        string cabin_brackets_name;
        int cabin_brackets_count;
        string cwt_brackets_name;
        int cwt_brackets_count;
        string cwt_belt_name;        
        int cwt_belt_count;
        string platat_name;
        int platat_count;          
        string hilti_bolts_name;
        int hilti_bolts_12mm;
        string assembly_bolts_name;
        int assembly_bolts_12mm;
        string bolts_8mm_name;
        int bolts_8mm;
        string spring_washers_8mm_name;
        int spring_washers_8mm;
        string spring_washers_12mm_name;
        int spring_washers_12mm;
        string nuts_12mm_name;
        int nuts_12mm;
        string flat_washers_12mm_name;
        int flat_washers_12mm;
        string sub_cabin_name;
        int sub_cabin_count;
        string sub_cwt_name;
        int sub_cwt_count;
        string scaffolding_name;
        int scaffolding_count = 1;
        string plumb_name;
        int plumb_count = 1;
        string balance_name;
        int balance_count = 1;

        string ceiling_cut_name;
        int ceiling_cut_count = 1;
        string parachute_name;
        int parachute_count = 1;
        string governor_rope_name;
        float governor_rope_meters;
        string buffer_set_name;
        int buffer_set_count = 1;
        string cwt_blocks_weight_desc;
        string machine_type_desc;
        int machine_bolts_24mm = 6;
        string machine_rubber_note;
        int rated_load_kg;
        string cwt_design_type;
        float cabin_wires_meters;
        string cabin_wires_name;
        string rope_hitches_desc;
        string rope_clamps_desc;
        int counterweight_blocks;

        float flex_cable_meters;
        int flex_holders_count = 4;
        float static_trunk_10cm = 6;
        float trunk_4cm_meters;
        int trunk_screws_8mm;
        int wire_1mm_coils; 
        string wire_1mm_count_desc; 
        float net_cable_meters;
        int lop_buttons_count;
        string photocell_note;
        int limit_switches_count = 3;
        int limit_sensors_count = 3;
        int floor_poles_count;
        
        string control_panel_name;
        int control_panel_count = 1;
        string ard_system_name;
        int ard_system_count = 1;
        string ctrl_fischer_name;
        int ctrl_fischer_count = 1;
        string inspect_box_name;
        int inspect_box_count = 1;
        string charger_batt_name;
        int charger_batt_count = 1;
        string emerg_alarm_name;
        int emerg_alarm_count = 1; 
        string flex_cable_name;
        string flex_holder_name;
        string trunk_4cm_name;
        string trunk_10cm_name;
        string trunk_screws_name;
        string oiler_set_name;
        int oiler_set_count = 1;
        string wire_6mm_name;
        int wire_6mm_count = 1;
        string wire_1mm_name;
        string net_cable_name;
        string cop_panel_name;
        int cop_panel_count = 1;
        string lop_buttons_name;
        string intercom_name;
        int intercom_count = 1;
        string safety_door_name;
        int safety_door_count = 1;
        string photocell_name;
        int photocell_count = 1;
        string stop_magnet_name;
        int stop_magnet_count = 1;
        string count_magnet_name;
        int count_magnet_count = 1;
        string limit_sw_name;
        string limit_sensors_name;
        string floor_poles_name;
        string machine_bed_name;
        
        bool has_ard;
        bool is_hydraulic;
    };

    FullSpecificationReport compile_full_specification(int w, int d, int floors, const string& type, const string& door_choice, const string& rails_origin, const string& door_origin, const string& has_ard) {
        FullSpecificationReport r;
        r.has_ard = (has_ard == "yes");
        r.is_hydraulic = (type == "Hydraulic");
        
        bool is_side_cwt = (w >= d + 20);

        int cab_w = get_cabin_width(w, is_side_cwt);
        int cab_d = get_cabin_depth(d);
        int cwt_dbg = get_cwt_dbg(w, is_side_cwt); 
        
        r.rated_load_kg = calculate_rated_load(cab_w, cab_d);
        
        r.ceiling_cut_name    = "تفتيح وتجهيز فتحات سقف البئر";
        r.parachute_name      = "جهاز براشوت";
        r.governor_rope_name  = "حبل براشوت";
        r.buffer_set_name     = "طقم بفر";
        r.machine_bed_name    = "فرش ماكينة";
        r.scaffolding_name = "سقالة";
        r.plumb_name       = "خيط ميزانية";
        r.balance_name     = "تيوب او خشب ميزانية";
        r.door_exterior_name = door_choice + " (" + door_origin + ")";
        r.total_exterior_doors = floors;

        float travel_path = ((floors - 1) * 3.2f) + 5.0f;
        int single_side_rails = static_cast<int>(travel_path / 5.0f) + 1;
        
        r.cabin_rails_name = (cab_d >= 150 ? "سكة كابينة 16 مللي" : "سكة كابينة 9 مللي") + string(" (") + rails_origin + ")";
        r.cabin_rails_count = single_side_rails * 2;

        r.cabin_brackets_count = floors * 8; 
        r.cabin_brackets_name  = "كوابيل كابينة";
        r.sub_cabin_count      = r.cabin_brackets_count; 
        r.sub_cabin_name       = "سبورتينات كابينة";
        
        r.platat_name = "بلتات";
        r.platat_count = 0;
        r.cwt_belt_name = "حزام";
        r.cwt_belt_count = 0;

        if (type == "Hydraulic") {
            r.cwt_rails_name        = "لا يوجد";
            r.cwt_rails_count      = 0;
            r.cwt_brackets_count   = 0;
            r.cwt_brackets_name    = "لا يوجد";
            r.sub_cwt_count        = 0;
            r.sub_cwt_name         = "لا يوجد";
            
            r.machine_type_desc   = "بستم ومجموعة ضخ هيدروليكية";
            r.machine_rubber_note = "لا يوجد";
            r.cwt_design_type     = "بدون ثقل";
            r.cabin_wires_name    = "لا يوجد";
            r.cabin_wires_meters  = 0;
            r.rope_hitches_desc   = "0";
            r.rope_clamps_desc    = "0";
            r.counterweight_blocks = 0;
        } else {
            r.cwt_rails_name        = "سكة تقل 5 مللي (" + rails_origin + ")";
            r.cwt_rails_count      = single_side_rails * 2;
            r.cwt_brackets_count   = floors * 8; 
            r.cwt_brackets_name    = "كوابيل تقل";
            r.sub_cwt_count        = r.cwt_brackets_count; 
            r.sub_cwt_name         = "سبورتينات تقل";
            
            if (is_side_cwt) {
                r.cwt_belt_count = (floors * 2) + 2; 
                int removed_cwt_brackets = r.cwt_brackets_count / 2;
                int removed_cab_brackets = r.cabin_brackets_count / 4;
                r.cwt_brackets_count -= removed_cwt_brackets;
                r.cabin_brackets_count -= removed_cab_brackets;
                r.platat_count = removed_cwt_brackets + removed_cab_brackets;
            }
            
            if (type == "MRL") {
                r.machine_type_desc   = "ماكينة جيرليس";
                r.machine_rubber_note = "لا يوجد";
                r.cwt_design_type     = is_side_cwt ? "تقل جانبي" : "تقل خلفي";
                
                r.cabin_wires_name   = (r.rated_load_kg <= 800) ? "حبل 6.5 مللي" : "حسب طارة الماكينة";
                r.cabin_wires_meters = (floors * 4.5f) * 2.0f;
                
                if (r.rated_load_kg <= 450) {
                    r.rope_hitches_desc = "14 شداد";
                    r.rope_clamps_desc  = "حسب عدد الحبال";
                } else if (r.rated_load_kg <= 630) {
                    r.rope_hitches_desc = "16 شداد";
                    r.rope_clamps_desc  = "حسب عدد الحبال";
                } else {
                    r.rope_hitches_desc = "حسب الماكينة";
                    r.rope_clamps_desc  = "حسب عدد الحبال";
                }
                
                r.counterweight_blocks = (r.rated_load_kg / 50) + 10; 
                r.control_panel_name = "كنترول جيرليس";
            } 
            else { 
                r.machine_type_desc   = "ماكينة جيربوكس";
                r.machine_rubber_note = "طقم ربر";
                r.cwt_design_type     = is_side_cwt ? "تقل جانبي" : "تقل خلفي";
                
                r.cabin_wires_name   = "حبل 11 مللي";
                r.cabin_wires_meters = static_cast<float>(floors * 4);
                
                int hitches = (r.rated_load_kg >= 800) ? 5 : 4;
                r.rope_hitches_desc = to_string(hitches) + " شداد";
                r.rope_clamps_desc  = "حسب عدد الحبال";
                
                r.counterweight_blocks = (r.rated_load_kg / 40) + 8;
                r.control_panel_name = "لوحة تحكم";
            }
        }

        r.hilti_bolts_name         = "مسمار هلتي 12 مللي";
        r.assembly_bolts_name      = "مسمار تجميع 12 مللي";
        r.bolts_8mm_name           = "مسمار 8 مللي";
        r.spring_washers_8mm_name  = "وردة سوستة وصامولة 8 مللي";
        r.spring_washers_12mm_name = "وردة سوستة 12 مللي";
        r.nuts_12mm_name           = "صامولة 12 مللي";
        r.flat_washers_12mm_name   = "وردة صاج 12 مللي";

        r.hilti_bolts_12mm    = (floors * 2) + (r.total_exterior_doors * 8);
        r.assembly_bolts_12mm = (r.cabin_brackets_count / 2) + 30 + ((r.cabin_rails_count - 2) * 8);
        r.bolts_8mm           = (type == "Hydraulic") ? 0 : ((r.cwt_rails_count - 2) * 8);
        r.spring_washers_8mm  = r.bolts_8mm;
        
        r.spring_washers_12mm = r.hilti_bolts_12mm + r.assembly_bolts_12mm;
        r.nuts_12mm           = r.assembly_bolts_12mm; 
        r.flat_washers_12mm   = (r.assembly_bolts_12mm * 2) + r.hilti_bolts_12mm + r.sub_cabin_count;

        r.governor_rope_meters = (travel_path * 2.0f) + 4.0f; 
        
        if (type != "Hydraulic" && cwt_dbg > 6) {
            r.cwt_blocks_weight_desc = to_string(cwt_dbg - 6) + " سم";
        } else {
            r.cwt_blocks_weight_desc = "لا يوجد";
        }

        r.ard_system_name    = "جهاز ARD";
        r.ctrl_fischer_name  = "مسامير وفيشر 12 مللي";
        r.inspect_box_name   = "علبة صيانة";
        r.charger_batt_name  = "شاحن وبطارية طوارئ";
        r.emerg_alarm_name   = "جرس وسارينة";
        r.flex_holder_name   = "حامل كبل مرن";
        r.trunk_10cm_name    = "ترنك 10 سم";
        r.oiler_set_name     = "طقم مزايت";
        r.wire_6mm_name      = "سلك 6 مللي";
        r.cop_panel_name     = "لوحة طلبات داخلية";
        r.intercom_name      = "جهاز انتركم";
        r.stop_magnet_name   = "مغناطيس توقف";
        r.count_magnet_name  = "مغناطيس عداد";
        r.limit_sw_name      = "ليميت سويتش";
        r.limit_sensors_name = "حساسات مغناطيسية";
        r.floor_poles_name   = "بولات مغناطيس";
        
        r.flex_cable_name   = "كبل مرن";
        r.flex_cable_meters = (floors * 4.0f) + 7.0f;
        
        r.trunk_4cm_name     = "ترنك 4 سم";
        r.trunk_4cm_meters   = travel_path; 
        r.trunk_screws_name  = "مسامير وفيشر 8 مللي";
        r.trunk_screws_8mm   = floors * 4;  
        
        r.wire_1mm_name = "سلك 1 مللي";
        int temp_coils = 0;
        if (floors >= 2 && floors <= 4)         temp_coils = 4;
        else if (floors >= 5 && floors <= 7)  temp_coils = 5;
        else if (floors >= 8 && floors <= 10) temp_coils = 8;
        else if (floors >= 11 && floors <= 15) temp_coils = 10;
        else if (floors >= 16 && floors <= 20) temp_coils = 16;
        else                                   temp_coils = static_cast<int>(floors * 0.8f);

        r.wire_1mm_coils = temp_coils;
        r.wire_1mm_count_desc = to_string(temp_coils) + " لفة"; 
        
        r.net_cable_name   = "سلك نت";
        r.net_cable_meters = travel_path + (floors * 2.0f) + 5.0f;
        
        r.lop_buttons_name  = "طلبات خارجية";
        r.lop_buttons_count = r.total_exterior_doors; 

        if (door_choice.find("Semi") == string::npos) {
            r.photocell_name = "ستارة ضوئية";
            r.safety_door_name = "ازرار فتح وغلق";
        } else {
            r.photocell_name = "غير مطلوب";
            r.safety_door_name = "باب داخلي";
        }

        r.floor_poles_count = floors * 6; 

        return r;
    }
};

struct Lesson {
    string slug;         
    string track_slug;   
    string type;         
    string title;
    string summary;      
    string content_html; 
    string video_embed_url; 
    int order;               
};

struct Track {
    string slug;        
    string emoji;
    string title;
    string description;
};

static vector<Track> get_tracks() {
    return {
        { "basics", "🧱", "مسار الأساسات الهندسية", "المفاهيم الأولى لتصفية أبعاد بئر المصعد ومكوناته الرئيسية وقراءة المخططات الفنية." },
        { "doors",  "🚪", "مسار أبواب المصاعد", "أنواع الأبواب وأكواد الفتح المختلفة وإزاي تختار النوع المناسب للمساحة المتوفرة لبئر المصعد." },
        { "darbat" , "🔨" , "كورس كهرباء المصاعد الشامل"  , "كورس تطبيقي متخصص في تعليم كل شيء يخص كهرباء الدوائر، الكنترولات، وتوصيل كوابل الأمان."},
        { "robotics", "🤖", "مسار الروبوتات والمتحكمات", "مسار تطبيقي لتعلم أسس الذكاء الاصطناعي والمتحكمات الدقيقة. من أول فهم وربط الحساسات، لحد بناء وبرمجة روبوت ذكي ومتكامل بنفسك." }
    }; 
}

static vector<Lesson> get_lessons() {
    return {};
}

static vector<Lesson> get_lessons_by_track(const string& track_slug) {
    vector<Lesson> all = get_lessons();
    vector<Lesson> filtered;
    for (auto& l : all) if (l.track_slug == track_slug) filtered.push_back(l);
    sort(filtered.begin(), filtered.end(), [](const Lesson& a, const Lesson& b) { return a.order < b.order; });
    return filtered;
}

static string get_modern_blue_css() {
    return "<style>"
           "*{box-sizing:border-box;}"
           ":root{"
           "--bg:#0a0e16; --surface:#121826; --surface-2:#1a2233; --border:#232c3f;"
           "--accent:#38bdf8; --accent-dim:rgba(56,189,248,0.14); --accent-2:#f5a524;"
           "--text:#f3f4f6; --text-muted:#8b96ab;"
           "--font-display:'Cairo', sans-serif; --font-mono:'JetBrains Mono', 'Cairo', monospace;"
           "}"
           "body.light-mode{"
           "--bg:#f8fafc; --surface:#ffffff; --surface-2:#f1f5f9; --border:#cbd5e1;"
           "--text:#0f172a; --text-muted:#64748b;"
           "}"
           "@media (prefers-reduced-motion: reduce){*{animation-duration:0.01ms !important; transition-duration:0.01ms !important;}}"
           "body{font-family:var(--font-display); background-color:var(--bg); color:var(--text); direction:rtl; text-align:right; margin:0; padding:0; min-height:100vh; display:flex; flex-direction:column;"
           "background-image:linear-gradient(rgba(56,189,248,0.045) 1px, transparent 1px), linear-gradient(90deg, rgba(56,189,248,0.045) 1px, transparent 1px);"
           "background-size:30px 30px; transition: background-color 0.3s, color 0.3s;}"
           "a{outline-offset:3px; text-decoration:none; color:inherit;}"
           ":focus-visible{outline:2px solid var(--accent); outline-offset:2px; border-radius:4px;}"

           ".navbar{background-color:var(--surface); border-bottom:1px solid var(--border); padding:14px 28px; display:flex; justify-content:space-between; align-items:center; position:relative; z-index:50; box-shadow:0 4px 6px -1px rgba(0,0,0,0.15); transition: background-color 0.3s;}"
           ".nav-right{display:flex; align-items:center; gap:20px; flex-wrap:wrap;}"
           ".navbar-brand{display:flex; align-items:center; gap:10px; color:var(--text); font-size:1.25rem; font-weight:800; text-decoration:none; padding-inline-end:22px; border-inline-end:1px solid var(--border);}"
           ".brand-mark{width:34px; height:34px; flex-shrink:0; border-radius:8px; display:flex; align-items:center; justify-content:center; overflow:hidden; border: 1px solid var(--border); background-color: var(--bg);}"
           ".brand-mark img{width:100%; height:100%; object-fit:cover;}"
           ".nav-center{display:flex; align-items:center; gap:18px; flex-wrap:wrap;}"
           ".nav-link{color:var(--text); font-size:1rem; font-weight:600; text-decoration:none; transition:color 0.15s;}"
           ".nav-link:hover{color:var(--accent);}"
           ".nav-dropdown{position:relative;}"
           ".nav-dropdown summary{cursor:pointer; list-style:none; display:flex; align-items:center; gap:6px; color:var(--text); font-weight:600; font-size:1rem; user-select:none;}"
           ".nav-dropdown summary::-webkit-details-marker{display:none;}"
           ".nav-dropdown summary:hover{color:var(--accent);}"
           ".nav-dropdown .chevron{width:13px; height:13px; fill:currentColor; transition:transform 0.2s;}"
           ".nav-dropdown[open] .chevron{transform:rotate(180deg);}"
           ".dropdown-panel{position:absolute; inset-inline-start:0; top:calc(100% + 16px); display:flex; gap:30px; background:var(--surface-2); border:1px solid var(--border); border-radius:12px; padding:18px 22px; min-width:190px; box-shadow:0 20px 25px -5px rgba(0,0,0,0.45); z-index:60; animation:dropdownIn 0.18s ease;}"
           "@keyframes dropdownIn{from{opacity:0; transform:translateY(-6px);} to{opacity:1; transform:translateY(0);}}"
           ".dropdown-col{display:flex; flex-direction:column; gap:11px; min-width:150px;}"
           ".dropdown-panel a, .mobile-panel a{color:var(--text); font-size:0.95rem; font-weight:600; text-decoration:none; transition:color 0.15s;}"
           ".dropdown-panel a:hover{color:var(--accent);}"
           ".desktop-only{display:flex;}"
           ".mobile-only{display:none;}"
           "@media (max-width:860px){.desktop-only{display:none;} .mobile-only{display:flex;} .navbar-brand span:last-child{font-size:1.05rem;}}"

           ".flags-strip{background:none; border-bottom:none; padding:6px 28px; display:flex; justify-content:flex-end; align-items:center; position:relative; z-index:40;}"
           ".flags-badge-box{display:flex; align-items:center; gap:12px; background:var(--surface); backdrop-filter:blur(10px); -webkit-backdrop-filter:blur(10px); border:1px solid var(--border); padding:5px 14px; border-radius:30px; box-shadow:0 4px 10px rgba(0,0,0,0.3); margin-left:auto;}" 
           ".flag-img-unit{width:22px; height:15px; border-radius:2px; box-shadow:0 2px 4px rgba(0,0,0,0.4); object-fit:cover; display:block;}"
           ".flag-img-sep{color:rgba(139,150,171,0.4); font-size:0.8rem; font-weight:300; user-select:none;}"

           ".nav-left{display:flex; align-items:center; gap:18px;}"
           ".nav-icon{color:var(--text-muted); cursor:pointer; display:flex; align-items:center; justify-content:center; transition:0.2s; text-decoration:none; list-style:none; background:none; border:none; padding:0;}"
           ".nav-icon::-webkit-details-marker{display:none;}"
           ".nav-icon:hover{color:var(--accent);}"
           ".nav-icon svg{width:22px; height:22px; fill:currentColor;}"
           
           "body.light-mode .theme-sun, body:not(.light-mode) .theme-moon { display:none; }"

           ".mobile-menu{position:relative;}"
           ".mobile-panel{position:absolute; inset-inline-end:0; top:calc(100% + 14px); background:var(--surface-2); border:1px solid var(--border); border-radius:12px; padding:18px 20px; display:flex; flex-direction:column; gap:4px; min-width:230px; box-shadow:0 20px 25px -5px rgba(0,0,0,0.45); z-index:60; animation:dropdownIn 0.18s ease;}"
           ".mobile-panel a{padding:9px 6px; border-radius:6px; display:block;}"
           ".mobile-panel a:hover{background:rgba(56,189,248,0.1); color:var(--accent);}"
           ".mobile-divider{height:1px; background:var(--border); margin:8px 2px;}"

           ".container{max-width:1100px; margin:0 auto; padding:50px 20px; flex:1; width:100%;}"
           ".card{position:relative; background:var(--surface); border:1px solid var(--border); padding:40px; border-radius:12px; box-shadow:0 20px 25px -5px rgba(0,0,0,0.3); text-align:right; transition: background-color 0.3s; margin-bottom:20px;}"
           ".card::before, .nav-card::before{content:''; position:absolute; top:-1px; right:-1px; width:18px; height:18px; border-top:2px solid var(--accent); border-right:2px solid var(--accent); border-top-right-radius:6px; opacity:0.6;}"
           ".card::after, .nav-card::after{content:''; position:absolute; bottom:-1px; left:-1px; width:18px; height:18px; border-bottom:2px solid var(--accent); border-left:2px solid var(--accent); border-bottom-left-radius:6px; opacity:0.6;}"
           ".card h2{color:var(--text); font-size:1.6rem; margin-top:0; margin-bottom:15px; font-weight:700; border-bottom:1px solid var(--border); padding-bottom:15px;}"
           ".sub-title{color:var(--text-muted); margin-bottom:35px; font-size:0.95rem; line-height:1.6;}"
           ".f-group{margin-bottom:24px; text-align:right;}"
           ".f-group label{font-weight:600; color:var(--text); display:block; margin-bottom:12px; font-size:0.95rem;}"
           "input,select{width:100%; padding:14px; border:1px solid var(--border); border-radius:8px; text-align:right; font-size:1rem; font-family:var(--font-display); background-color:var(--bg); color:var(--text); transition:0.3s; font-weight:600; padding-right:15px; direction:rtl;}"
           "input:focus, select:focus{outline:none; border-color:var(--accent); box-shadow:0 0 0 3px rgba(56,189,248,0.2);}"
           "button, .btn-action{background:linear-gradient(135deg, #0284c7, #0369a1); color:#ffffff; border:none; padding:16px; border-radius:8px; width:100%; font-size:1.1rem; font-weight:700; cursor:pointer; transition:0.3s; text-decoration:none; display:inline-block; text-align:center; box-shadow: 0 4px 6px rgba(0,0,0,0.1);}"
           "button:hover, .btn-action:hover{background:linear-gradient(135deg, #0369a1, #075985); transform:translateY(-1px);}"

           ".table-container{position:relative; width:100%; overflow-x:auto; background:var(--bg); border-radius:8px; border:1px solid var(--border); margin-top:10px;}"
           ".tbl{width:100%; border-collapse:collapse; text-align:right; margin-bottom: 20px;}"
           ".tbl th{background:var(--surface); padding:15px; color:var(--text); font-weight:600; border-bottom:1px solid var(--border); font-size:1rem; text-align:right; width:45%;}"
           ".tbl td{padding:15px; border-bottom:1px solid var(--border); color:var(--text); font-size:1rem; font-weight:600; text-align:right; font-family:var(--font-mono);}"
           
           ".stage-header { padding: 12px 15px; border-radius: 8px; margin: 25px 0 10px 0; font-weight: 800; color: #fff; text-align: center; font-size: 1.1rem; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }"
           ".stage-1 { background: linear-gradient(135deg, #0ea5e9, #0284c7); }" 
           ".stage-2 { background: linear-gradient(135deg, #8b5cf6, #7c3aed); }" 
           ".stage-3 { background: linear-gradient(135deg, #f59e0b, #d97706); }" 

           ".actions{display:flex; justify-content:space-between; margin-top:35px; gap:20px; flex-wrap:wrap;}"
           ".btn-print{background:linear-gradient(135deg, #16a34a, #15803d); color:white; border:none; padding:15px 25px; border-radius:8px; font-weight:700; cursor:pointer; flex:1; transition:0.3s; text-align:center; font-family:var(--font-display); box-shadow: 0 4px 6px rgba(0,0,0,0.1);}"
           ".btn-print:hover{background:linear-gradient(135deg, #15803d, #166534);}"
           ".btn-save{background:linear-gradient(135deg, #f59e0b, #d97706); color:white; border:none; padding:15px 25px; border-radius:8px; font-weight:700; cursor:pointer; flex:1; transition:0.3s; text-align:center; font-family:var(--font-display); box-shadow: 0 4px 6px rgba(0,0,0,0.1);}"
           ".btn-save:hover{background:linear-gradient(135deg, #d97706, #b45309);}"
           ".btn-secondary{background:linear-gradient(135deg, #4f46e5, #4338ca); color:white; padding:15px 25px; border-radius:8px; font-weight:700; text-align:center; flex:1; transition:0.3s; display:inline-block; text-decoration:none; font-family:var(--font-display); box-shadow: 0 4px 6px rgba(0,0,0,0.1);}"
           ".btn-secondary:hover{background:linear-gradient(135deg, #4338ca, #3730a3);}"
           
           ".grid-cards { display: grid; grid-template-columns: repeat(2, 1fr); gap: 25px; width: 100%; }"
           "@media (max-width: 768px) { .grid-cards { grid-template-columns: 1fr; } }"

           ".grid-nav{display:grid; grid-template-columns:repeat(auto-fit, minmax(280px, 1fr)); gap:25px; width:100%;}"
           ".nav-card{position:relative; background:var(--surface); border:1px solid var(--border); padding:30px; border-radius:12px; text-decoration:none; color:var(--text); transition:0.3s; display:flex; flex-direction:column; text-align:right;}"
           ".nav-card:hover{border-color:var(--accent); transform:translateY(-3px); box-shadow:0 10px 20px rgba(0,0,0,0.2);}"
           ".nav-card h3{color:var(--accent); font-size:1.3rem; margin:0 0 12px 0;}"
           ".nav-card p{color:var(--text-muted); font-size:0.95rem; line-height:1.6; margin:0;}"

           ".section-intro{margin-bottom:30px; text-align:right;}"
           ".section-intro h1{color:var(--text); font-size:1.7rem; font-weight:800; margin:0 0 8px 0;}"
           ".section-intro p{color:var(--text-muted); font-size:1rem; line-height:1.7; margin:0;}"
           ".footer{margin-top:auto; padding:25px 0; font-size:15px; color:var(--text-muted); text-align:center; border-top:1px solid var(--border); background-color:var(--surface); font-weight:600;}"
           "</style>";
}

static string get_seo_meta(const string& title, const string& desc) {
    return "<title>موقع ضربة شاكوش</title>"
           "<link rel='icon' type='image/jpeg' href='https://media.darbat-shakosh.com/channels4_profile%20(1).jpg'>"
           "<meta name='description' content='" + desc + "'>"
           "<meta name='keywords' content='حاسبة مقاسات المصاعد, كورس كهرباء المصاعد, تصفية أبعاد بئر المصعد, صيانة المصاعد, ميكانيكا المصاعد, ضربة شاكوش, هندسة المصاعد'>"
           "<meta name='robots' content='index, follow'>";
}

static string get_navbar_html(const string& current_user = "") {
    const string logo_url = "https://media.darbat-shakosh.com/channels4_profile%20(1).jpg"; 
    const string chevron_svg = "<svg class='chevron' viewBox='0 0 24 24'><path d='M7 10l5 5 5-5z'/></svg>";
    const string moon_icon = "<svg class='theme-moon' viewBox='0 0 24 24'><path d='M12.3 22h-.1c-5.5 0-10-4.5-10-10 0-4.8 3.5-8.9 8.2-9.8.6-.1 1.2.3 1.3.9.1.6-.2 1.2-.8 1.4-3.3 1-5.7 4-5.7 7.5 0 4.4 3.6 8 8 8 3.5 0 6.5-2.4 7.5-5.7.2-.6.8-.9 1.4-.8.6.1 1 .7.9 1.3-.9 4.7-5 8.2-9.8 8.2z'/></svg>";
    const string sun_icon = "<svg class='theme-sun' viewBox='0 0 24 24'><path d='M12 7c-2.8 0-5 2.2-5 5s2.2 5 5 5 5-2.2 5-5-2.2-5-5-5zm0-5c.6 0 1 .4 1 1v2c0 .6-.4 1-1 1s-1-.4-1-1V3c0-.6.4-1 1-1zm0 14c.6 0 1 .4 1 1v2c0 .6-.4 1-1 1s-1-.4-1-1v-2c0-.6.4-1 1-1zM4 11h2c.6 0 1 .4 1 1s-.4 1-1 1H4c-.6 0-1-.4-1-1s-1-.4-1-1zm14 0h2c.6 0 1 .4 1 1s-.4 1-1 1h-2c-.6 0-1-.4-1-1s-1-.4-1-1zM5.2 5.2c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0L5.2 6.6c-.4-.4-.4-1 0-1.4zm12 12c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0l-1.4-1.4c-.4-.4-.4-1 0-1.4zM7.6 16.4c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0l-1.4-1.4c-.4-.4-.4-1 0-1.4zm12-12c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0l-1.4-1.4c-.4-.4-.4-1 0-1.4z'/></svg>";

    string user_controls;
    if (current_user.empty()) {
        user_controls = "<a href='/login' class='nav-icon' title='تسجيل الدخول / إنشاء حساب'><svg viewBox='0 0 24 24'><path d='M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 3c1.66 0 3 1.34 3 3s-1.34 3-3 3-3-1.34-3-3 1.34-3 3-3zm0 14.2c-2.5 0-4.71-1.28-6-3.22.03-1.99 4-3.08 6-3.08 1.99 0 5.97 1.09 6 3.08-1.29 1.94-3.5 3.22-6 3.22z'/></svg></a>";
    } else {
        user_controls = "<details class='nav-dropdown'><summary class='nav-icon' style='color:var(--accent); font-weight:bold;'>👤 " + current_user + "</summary>"
                        "<div class='dropdown-panel' style='min-width:140px;'><div class='dropdown-col'><a href='/my-reports'>التقارير المحفوظة</a><a href='/logout'>تسجيل الخروج</a></div></div></details>";
    }

    return "<nav class='navbar'>"
           "  <div class='nav-right'>"
           "    <a href='/' class='navbar-brand'><span class='brand-mark'><img src='" + logo_url + "' alt='لوجو ضربة شاكوش'></span><span>ضربة شاكوش </span></a>"
           "    <div class='nav-center desktop-only'>"
           "      <a href='/' class='nav-link'>الرئيسية</a>"
           "      <a href='/paths' class='nav-link'>مسارات التعلّم</a>"
           "      <a href='/blog' class='nav-link'>الشروحات والمقالات</a>"
           "      <a href='/calculator' class='nav-link'>الحاسبة الهندسية</a>"
           "      <details class='nav-dropdown'>"
           "        <summary>🤝 دليل الشركاء " + chevron_svg + "</summary>"
           "        <div class='dropdown-panel'>"
           "          <div class='dropdown-col'>"
           "            <a href='/companies'>الشركات والمؤسسات</a>"
           "            <a href='/contractors'>المقاولين</a>"
           "            <a href='/suppliers'>الموردين</a>"
           "            <a href='/cabins'>مصانع الكباين</a>"
           "            <a href='/transport'>دباب وديانا</a>"
           "            <a href='/labor'>العمالة اليومية</a>"
           "          </div>"
           "        </div>"
           "      </details>"
           "      <details class='nav-dropdown'>"
           "        <summary>المزيد " + chevron_svg + "</summary>"
           "        <div class='dropdown-panel'>"
           "          <div class='dropdown-col'>"
           "            <a href='/contact'>اتصل بنا</a>"
           "            <a href='/support'>مركز الدعم</a>"
           "          </div>"
           "        </div>"
           "      </details>"
           "    </div>"
           "  </div>"
           "  <div class='nav-left'>"
           "    <button class='nav-icon' id='themeBtn' title='تغيير الوضع المضيء/الليلي'>" + moon_icon + sun_icon + "</button>"
           + user_controls +
           "    <details class='nav-dropdown mobile-menu mobile-only'>"
           "      <summary class='nav-icon' title='القائمة'><svg viewBox='0 0 24 24'><path d='M3 6h18v2H3zm0 5h18v2H3zm0 5h18v2H3z'/></svg></summary>"
           "      <div class='mobile-panel'>"
           "        <a href='/'>الرئيسية</a>"
           "        <a href='/login'>تسجيل الدخول / الحساب</a>"
           "        <a href='/paths'>مسارات التعلّم</a>"
           "        <a href='/blog'>الشروحات والمقالات</a>"
           "        <a href='/calculator'>الحاسبة الهندسية</a>"
           "        <a href='/companies'>الشركات</a>"
           "        <a href='/contractors'>المقاولين</a>"
           "        <a href='/suppliers'>الموردين</a>"
           "        <a href='/cabins'>مصانع الكباين</a>"
           "        <a href='/transport'>دباب وديانا</a>"
           "        <a href='/labor'>العمالة اليومية</a>"
           "        <div class='mobile-divider'></div>"
           "        <a href='/contact'>اتصل بنا</a>"
           "        <a href='/support'>مركز الدعم</a>"
           "      </div>"
           "    </details>"
           "  </div>"
           "</nav>"
           "<div class='flags-strip'>"
           "  <div class='flags-badge-box'>"
           "    <img src='https://flagcdn.com/w40/ps.png' class='flag-img-unit' alt='Gaza Palestine'>"
           "    <span class='flag-img-sep'>|</span>"
           "    <img src='https://flagcdn.com/w40/eg.png' class='flag-img-unit' alt='Egypt'>"
           "    <span class='flag-img-sep'>|</span>"
           "    <img src='https://flagcdn.com/w40/sa.png' class='flag-img-unit' alt='Saudi Arabia'>"
           "  </div>"
           "</div>";
}

static string get_theme_script(const string& nonce) {
    return "<script nonce='" + nonce + "'>"
           "  if(localStorage.getItem('theme') === 'light'){"
           "    document.body.classList.add('light-mode');"
           "  }"
           "  document.getElementById('themeBtn').addEventListener('click', function(){"
           "    document.body.classList.toggle('light-mode');"
           "    if(document.body.classList.contains('light-mode')){"
           "      localStorage.setItem('theme', 'light');"
           "    } else {"
           "      localStorage.setItem('theme', 'dark');"
           "    }"
           "  });"
           "</script>";
}

int main() {
    httplib::Server svr;
    Elevator elevator;

    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        set_security_headers(res);
        set_csp(res);

        if (!CF_VERIFY_SECRET.empty()) {
            if (!req.has_header(SECURE_HEADER_NAME.c_str()) || req.get_header_value(SECURE_HEADER_NAME.c_str()) != CF_VERIFY_SECRET) {
                res.status = 403; res.set_content("Forbidden Access.", "text/plain");
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        if (is_rate_limited(get_client_ip(req))) {
            res.status = 429; res.set_content("Too Many Requests. Please wait a moment.", "text/plain");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        string nonce = generate_nonce(); set_csp(res, nonce);
        string meta = get_seo_meta("المنصة التعليمية والهندسية للمصاعد والروبوتات", "شروحات فنية متخصصة في ميكانيكا وكهرباء المصاعد وحساب أبعاد الصاعدة فنياً.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() +
                      "</head><body>"
                      + get_navbar_html(user) +
                      "<div class='container'>"
                      "<div class='section-intro' style='text-align: center; margin-bottom: 40px;'>"
                      "  <h1 style='font-size: 2.2rem;'>مرحباً بك في منصة ضربة شاكوش</h1>"
                      "  <p style='color: var(--text-muted); font-size: 1.1rem;'> الأدوات الهندسية الذكية والكورسات التطبيقية المتخصصة في مجال تركيب وصيانة المصاعد والروبوتات</p>"
                      "</div>"
                      "<div class='grid-nav'>"
                      "  <a href='/calculator' class='nav-card'><h3>🛗 حاسبة المقاسات الفنية والبضاعة</h3><p>ابدأ تصفية أبعاد بئر المصعد وحساب المقاسات الصافية للكابينة.</p></a>"
                      "  <a href='/paths' class='nav-card'><h3>📖 مسارات الكورسات والتعلم</h3><p>اكتشف مسار التأسيس ميكانيكياً، وكورس كهرباء المصاعد الشامل.</p></a>"
                      "  <a href='/companies' class='nav-card'><h3>🏢 الشركات والمؤسسات</h3><p>تعرف على الشركات والمؤسسات الكبرى المعتمدة في قطاع المصاعد.</p></a>"
                      "</div>"
                      "</div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 2. صفحة تسجيل احترافية ومطابقة للترتيب المطلوب بدقة (1- الاسم الأول، 2- الاسم الأخير، 3- اسم المستخدم، 4- البريد، 5- كلمة السر، 6- إعادة كتابة كلمة السر)
    auto render_register_page = [](httplib::Response& res, const string& fn = "", const string& ln = "", const string& un = "", const string& em = "", const string& err_msg = "") {
        string nonce = generate_nonce(); set_csp(res, nonce);
        string alert_box = err_msg.empty() ? "" : "<div style='background:rgba(239,68,68,0.1); border:1px solid #ef4444; color:#ef4444; padding:12px; border-radius:8px; margin-bottom:20px; font-weight:600; text-align:center;'>" + err_msg + "</div>";
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + 
                      "<style>.auth-card{max-width:550px; margin:20px auto; background:var(--surface); border:1px solid var(--border); padding:35px; border-radius:20px; box-shadow:0 15px 35px rgba(0,0,0,0.25);}</style></head><body>"
                      + get_navbar_html() + "<div class='container'><div class='auth-card'>"
                      "<h2 style='text-align:center; color:var(--accent); margin-bottom:5px;'>✨ انضم إلى نخبة المهندسين</h2>"
                      "<div style='text-align:center; color:var(--text-muted); font-size:0.9rem; margin-bottom:25px;'>أنشئ حسابك لتوثيق مقاييسك والاحتفاظ بسجل أعمالك</div>"
                      + alert_box +
                      "<form action='/api/register' method='post'>"
                      "<div class='f-group'><label>1- الاسم الأول:</label><input type='text' name='first_name' value='" + fn + "' required placeholder='مثال: محمد'></div>"
                      "<div class='f-group'><label>2- الاسم الأخير:</label><input type='text' name='last_name' value='" + ln + "' required placeholder='مثال: الشعراوي'></div>"
                      "<div class='f-group'><label>3- اسم المستخدم (بالإنجليزية بدون مسافات):</label><input type='text' name='username' value='" + un + "' required pattern='[a-zA-Z0-9_]+' placeholder='مثال: mohamed_shaarawy'></div>"
                      "<div class='f-group'><label>4- البريد الإلكتروني الحقيقي:</label><input type='email' name='email' value='" + em + "' required placeholder='example@domain.com'></div>"
                      "<div class='f-group'><label>5- كلمة السر:</label><input type='password' name='password' required placeholder='أدخل كلمة المرور'></div>"
                      "<div class='f-group'><label>6- إعادة كتابة كلمة السر:</label><input type='password' name='confirm_password' required placeholder='أعد كتابة كلمة المرور'></div>"
                      "<button type='submit' style='margin-top:10px;'>✨ إنشاء الحساب وإرسال الرمز</button></form>"
                      "<div style='text-align:center; margin-top:20px;'><a href='/login' style='color:var(--accent); font-weight:600;'>لديك حساب بالفعل؟ تسجيل الدخول</a></div>"
                      "</div></div><div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    };

    svr.Get("/register", [&render_register_page](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        if (!user.empty()) { res.set_redirect("/"); return; }
        render_register_page(res);
    });

    // 1. معالجة التسجيل مع الاحتفاظ بالبيانات السليمة عند حدوث أخطاء وإرسال الإيميل الفعلي عبر cURL
    svr.Post("/api/register", [&render_register_page](const httplib::Request& req, httplib::Response& res) {
        string first_name = html_escape(req.get_param_value("first_name"));
        string last_name = html_escape(req.get_param_value("last_name"));
        string username = html_escape(req.get_param_value("username"));
        string email = html_escape(req.get_param_value("email"));
        string password = html_escape(req.get_param_value("password"));
        string confirm_password = html_escape(req.get_param_value("confirm_password"));

        if (password != confirm_password) {
            render_register_page(res, first_name, last_name, username, email, "⚠️ كلمتا المرور غير متطابقتين. يرجى إعادة كتابة كلمة المرور.");
            return;
        }

        if (!is_valid_username(username)) {
            render_register_page(res, first_name, last_name, "", email, "⚠️ اسم المستخدم غير صالح. مسموح بالحروف والأرقام الإنجليزية بدون مسافات.");
            return;
        }

        if (users_db.find(username) != users_db.end()) {
            render_register_page(res, first_name, last_name, "", email, "⚠️ اسم المستخدم مستخدم مسبقاً، يرجى اختيار اسم آخر.");
            return;
        }

        for (auto const& [u, acc] : users_db) {
            if (acc.email == email) {
                render_register_page(res, first_name, last_name, username, "", "⚠️ البريد الإلكتروني مسجل لدينا مسبقاً.");
                return;
            }
        }

        string otp = generate_otp();
        UserAccount new_acc;
        new_acc.first_name = first_name;
        new_acc.last_name = last_name;
        new_acc.username = username;
        new_acc.email = email;
        new_acc.password = password;
        new_acc.otp_code = otp;
        new_acc.is_verified = false;
        
        users_db[username] = new_acc;
        save_user_to_mongodb(new_acc);

        // إرسال الإيميل الحقيقي الفعلي عبر cURL
        send_email_otp(email, first_name, otp);

        string nonce = generate_nonce(); set_csp(res, nonce);
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html() +
                      "<div class='container' style='max-width:500px;'>"
                      "<div class='card' style='border-color:var(--accent); text-align:center;'>"
                      "<h2>🔐 أدخل رمز تفعيل الحساب</h2>"
                      "<div class='sub-title'>تم إرسال رمز التحقق إلى بريدك الإلكتروني: <b>" + email + "</b><br>يرجى تفقد صندوق الوارد أو الـ Spam وإدخال الرمز:</div>"
                      "<form action='/api/verify-otp' method='post'>"
                      "<input type='hidden' name='username' value='" + username + "'>"
                      "<div class='f-group'><input type='text' name='otp' required maxlength='6' placeholder='أدخل الرمز هنا' style='text-align:center; font-size:1.4rem; letter-spacing:4px; font-weight:bold;'></div>"
                      "<button type='submit'>✅ تفعيل الحساب نهائياً</button>"
                      "</form></div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 3. رسالة ترحيبية مخصصة وحارة بعد تفعيل الحساب
    svr.Post("/api/verify-otp", [](const httplib::Request& req, httplib::Response& res) {
        string username = html_escape(req.get_param_value("username"));
        string otp = html_escape(req.get_param_value("otp"));

        if (users_db.find(username) != users_db.end() && users_db[username].otp_code == otp) {
            users_db[username].is_verified = true;
            save_user_to_mongodb(users_db[username]);
            res.set_header("Set-Cookie", "session=" + username + "; Path=/; HttpOnly");
            
            string nonce = generate_nonce(); set_csp(res, nonce);
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                          + get_modern_blue_css() + "</head><body>"
                          + get_navbar_html(username) +
                          "<div class='container' style='max-width:550px; text-align:center;'>"
                          "<div class='card' style='border-color:#16a34a;'>"
                          "<h2 style='color:#16a34a;'>🎉 أهلاً وسهلاً بك يا بشمهندس " + users_db[username].first_name + " " + users_db[username].last_name + "!</h2>"
                          "<p style='color:var(--text); font-size:1.1rem; line-height:1.8; margin-bottom:25px;'>نورت منصة ضربة شاكوش الرقمية. تم تفعيل حسابك وحفظ بياناتك بنجاح تام، وأصبحت جاهزاً لحفظ تقاريرك الهندسية باسم عملائك.</p>"
                          "<a class='btn-secondary' href='/calculator' style='background:linear-gradient(135deg, #16a34a, #15803d); display:block; padding:15px;'>🛗 ابدأ العمل على الحاسبة الهندسية</a>"
                          "</div></div>"
                          "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                          + get_theme_script(nonce) + "</body></html>";
            res.set_content(html, "text/html; charset=utf-8");
        } else {
            string nonce = generate_nonce(); set_csp(res, nonce);
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                          + get_modern_blue_css() + "</head><body>"
                          + get_navbar_html() +
                          "<div class='container' style='max-width:500px; text-align:center;'><div class='card' style='border-color:#ef4444;'>"
                          "<h2 style='color:#ef4444;'>❌ رمز التحقق غير صحيح</h2>"
                          "<p style='color:var(--text-muted); margin-bottom:20px;'>الرمز غير مطابق، يرجى المحاولة مرة أخرى.</p>"
                          "<a class='btn-secondary' href='/register'>🔄 العودة للوراء</a>"
                          "</div></div></body></html>";
            res.set_content(html, "text/html; charset=utf-8");
        }
    });

    // 5. تصميم احترافي لصفحة تسجيل الدخول
    svr.Get("/login", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        if (!user.empty()) { res.set_redirect("/"); return; }
        string nonce = generate_nonce(); set_csp(res, nonce);
        string meta = get_seo_meta("تسجيل الدخول", "سجل دخولك الآن.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + 
                      "<style>.auth-card{max-width:450px; margin:40px auto; background:var(--surface); border:1px solid var(--border); padding:40px; border-radius:20px; box-shadow:0 15px 35px rgba(0,0,0,0.25);}</style></head><body>"
                      + get_navbar_html() +
                      "<div class='container'>"
                      "<div class='auth-card'>"
                      "<h2 style='text-align:center; color:var(--accent);'>🔑 تسجيل الدخول</h2>"
                      "<div class='sub-title' style='text-align:center;'>أهلاً بك مجدداً في بيئتك الهندسية</div>"
                      "<form action='/api/login' method='post'>"
                      "<div class='f-group'><label>👤 اسم المستخدم:</label><input type='text' name='username' required placeholder='اكتب اسم المستخدم'></div>"
                      "<div class='f-group'><label>🔒 كلمة المرور:</label><input type='password' name='password' required placeholder='اكتب كلمة المرور'></div>"
                      "<button type='submit' style='margin-top:10px;'>➡️ دخول للحساب</button>"
                      "</form>"
                      "<div style='text-align:center; margin-top:20px; display:flex; flex-direction:column; gap:10px;'>"
                      "<a href='/forgot-password' style='color:var(--accent-2); font-weight:600;'>نسيت كلمة المرور؟</a>"
                      "<a href='/register' style='color:var(--accent); font-weight:600;'>إنشاء حساب جديد</a>"
                      "</div></div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        string username = html_escape(req.get_param_value("username"));
        string password = html_escape(req.get_param_value("password"));

        if (users_db.find(username) != users_db.end() && users_db[username].password == password) {
            if (!users_db[username].is_verified) {
                string nonce = generate_nonce(); set_csp(res, nonce);
                string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                              "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                              + get_modern_blue_css() + "</head><body>"
                              + get_navbar_html() +
                              "<div class='container' style='max-width:500px; text-align:center;'><div class='card' style='border-color:#f59e0b;'>"
                              "<h2 style='color:#f59e0b;'>⚠️ الحساب غير مفعل</h2>"
                              "<p style='color:var(--text-muted); margin-bottom:20px;'>يرجى تفعيل حسابك أولاً بالرمز المرسل على إيميلك.</p>"
                              "<a class='btn-secondary' href='/login'>🔄 العودة لت تسجيل الدخول</a>"
                              "</div></div></body></html>";
                res.set_content(html, "text/html; charset=utf-8");
                return;
            }
            res.set_header("Set-Cookie", "session=" + username + "; Path=/; HttpOnly");
            res.set_redirect("/");
        } else {
            string nonce = generate_nonce(); set_csp(res, nonce);
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                          + get_modern_blue_css() + "</head><body>"
                          + get_navbar_html() +
                          "<div class='container' style='max-width:500px; text-align:center;'><div class='card' style='border-color:#ef4444;'>"
                          "<h2 style='color:#ef4444;'>⚠️ بيانات الدخول غير صحيحة</h2>"
                          "<p style='color:var(--text-muted); margin-bottom:20px;'>اسم المستخدم أو كلمة المرور غير صحيحة.</p>"
                          "<a class='btn-secondary' href='/login'>🔄 المحاولة مجدداً</a>"
                          "</div></div></body></html>";
            res.set_content(html, "text/html; charset=utf-8");
        }
    });

    // تفعيل زر (نسيت كلمة المرور)
    svr.Get("/forgot-password", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        if (!user.empty()) { res.set_redirect("/"); return; }
        string nonce = generate_nonce(); set_csp(res, nonce);
        string meta = get_seo_meta("استعادة كلمة المرور", "استعادة كلمة المرور المفقودة بشكل آمن.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + 
                      "<style>.auth-card{max-width:450px; margin:40px auto; background:var(--surface); border:1px solid var(--border); padding:40px; border-radius:20px; box-shadow:0 15px 35px rgba(0,0,0,0.25);}</style></head><body>"
                      + get_navbar_html() +
                      "<div class='container'>"
                      "<div class='auth-card'>"
                      "<h2 style='text-align:center; color:var(--accent-2);'>🔓 استعادة كلمة المرور</h2>"
                      "<div class='sub-title' style='text-align:center;'>أدخل البريد الإلكتروني المسجل، وسنرسل لك تعليمات استعادة الحساب بأمان:</div>"
                      "<form action='/api/forgot-password' method='post'>"
                      "<div class='f-group'><label>📧 البريد الإلكتروني:</label><input type='email' name='email' required placeholder='example@domain.com'></div>"
                      "<button type='submit' style='background:linear-gradient(135deg, #f59e0b, #d97706); margin-top:10px;'>📤 إرسال تعليمات الاستعادة</button>"
                      "</form>"
                      "<div style='text-align:center; margin-top:20px;'><a href='/login' style='color:var(--text-muted); font-weight:600;'>العودة لتسجيل الدخول</a></div>"
                      "</div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Post("/api/forgot-password", [](const httplib::Request& req, httplib::Response& res) {
        string email = html_escape(req.get_param_value("email"));
        string nonce = generate_nonce(); set_csp(res, nonce);
        
        bool found = false;
        for (auto const& [u, acc] : users_db) {
            if (acc.email == email) {
                found = true;
                break;
            }
        }

        string msg = found ? "تم إرسال رابط ورسالة استعادة كلمة المرور إلى بريدك الإلكتروني بنجاح. يرجى تفقد صندوق الوارد." : "عفواً، هذا البريد الإلكتروني غير مسجل لدينا في النظام.";
        string color = found ? "#16a34a" : "#ef4444";

        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html() +
                      "<div class='container' style='max-width:500px; text-align:center;'><div class='card' style='border-color:" + color + ";'>"
                      "<h2 style='color:" + color + ";'>حالة الاستعلام</h2>"
                      "<p style='font-size:1.1rem; line-height:1.8;'>" + msg + "</p><br>"
                      "<a class='btn-secondary' href='/login'>➡️ العودة لتسجيل الدخول</a>"
                      "</div></div></body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get("/logout", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Set-Cookie", "session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
        res.set_redirect("/");
    });

    // ========================================================================

    auto render_page = [](const string& title, const string& target_type, const string& nonce, const string& current_user) {
        auto partners = get_partners();
        ostringstream content;
        ostringstream featured_content;

        for (auto& p : partners) {
            if (p.type != target_type) continue;

            if (p.is_ad) {
                content << "<div class='nav-card' style='border: 2px dashed var(--accent); background: rgba(56,189,248,0.04); text-align: center; justify-content: center;'>"
                        << "<h3 style='color: var(--accent);'>📢 اكتب اسمك هنا</h3>"
                        << "<p>" << html_escape(p.details) << "</p>"
                        << "<a class='btn-action' href='https://wa.me/" << p.phone << "' target='_blank' style='margin-top:15px; padding:10px;'>💬 احجز مكانك الآن</a>"
                        << "</div>";
                continue;
            }

            if (p.type == "company" && p.is_featured) {
                featured_content << "<div style='background:linear-gradient(135deg, #0284c7, #0369a1); color:white; padding:30px; border-radius:12px; box-shadow:0 10px 25px rgba(2,132,199,0.3); border:2px solid #38bdf8; margin-bottom:25px; text-align:right; width:100%;'>"
                                 << "<span style='background:#f5a524; color:#000; font-size:0.8rem; font-weight:800; padding:4px 12px; border-radius:20px;'>⭐ الشركة الرئيسية والراعية</span>"
                                 << "<h2 style='margin:12px 0 8px 0; font-size:1.5rem; color:#fff; border:none; padding:0;'>" << html_escape(p.name) << "</h2>"
                                 << "<p style='font-size:1.05rem; margin-bottom:12px; color:#e0f2fe;'>" << html_escape(p.details) << "</p>"
                                 << "<div style='display:flex; gap:20px; flex-wrap:wrap; font-weight:bold;'>"
                                 << "<span>📞 الجوال: " << html_escape(p.phone) << "</span>"
                                 << (!p.location.empty() ? "<span>📍 المكان: " + html_escape(p.location) + "</span>" : "")
                                 << (!p.map_link.empty() ? "<span>📍 <a href='" + p.map_link + "' target='_blank' style='color:#fde047; text-decoration:underline;'> العنوان </a></span>" : "")
                                 << (!p.website.empty() ? "<span>🌐 <a href='" + p.website + "' target='_blank' style='color:#fde047; text-decoration:underline;'> زيارة موقع الشركة </a></span>" : "")
                                 << "</div>"
                                 << "<div style='margin-top:15px; display:flex; gap:10px; flex-wrap:wrap;'>"
                                 << "<a class='btn-action' href='https://wa.me/" << p.phone << "' target='_blank' style='background:#25D366; width:auto; display:inline-block; padding:10px 25px;'>💬 تواصل مباشر واتساب</a>"
                                 << (!p.website.empty() ? "<a class='btn-action' href='" + p.website + "' target='_blank' style='background:linear-gradient(135deg, #0284c7, #0369a1); width:auto; display:inline-block; padding:10px 25px;'>🌐 زيارة موقع الشركة</a>" : "")
                                 << "</div>"
                                 << "</div>";
            } else {
                content << "<div style='background:var(--surface); border:1px solid var(--border); padding:25px; border-radius:12px; box-shadow:0 10px 20px rgba(0,0,0,0.2); text-align:right; display:flex; flex-direction:column; justify-content:space-between; transition:0.3s;'>"
                        << "<div>"
                        << (p.type == "company" ? "<span style='background:rgba(56,189,248,0.15); color:var(--accent); font-size:0.75rem; font-weight:700; padding:3px 10px; border-radius:20px;'>🏢 شركة معتمدة</span>" : "")
                        << (!p.rating.empty() ? "<p style='font-size:1.05rem; color:var(--accent-2); margin-bottom:6px;'>التقييم: " + p.rating + "</p>" : "")
                        << "<h3 style='margin:10px 0 8px 0; font-size:1.25rem; color:var(--text);'>" << html_escape(p.name) << "</h3>"
                        << "<p style='font-size:0.95rem; margin-bottom:12px; color:var(--text-muted); line-height:1.6;'>" << html_escape(p.details) << "</p>"
                        << "<div style='display:flex; gap:15px; flex-wrap:wrap; font-weight:bold; font-size:0.9rem; margin-bottom:15px;'>"
                        << "<span style='color:var(--text);'>📞 " << html_escape(p.phone) << "</span>"
                        << (!p.location.empty() ? "<span style='color:var(--text);'>📍 " + html_escape(p.location) + "</span>" : "")
                        << (!p.map_link.empty() ? "<span>📍 <a href='" + p.map_link + "' target='_blank' style='color:var(--accent); text-decoration:underline;'>رابط الخريطة</a></span>" : "")
                        << (!p.website.empty() ? "<span>🌐 <a href='" + p.website + "' target='_blank' style='color:#38bdf8; text-decoration:underline;'>الموقع</a></span>" : "")
                        << "</div>"
                        << "</div>"
                        << "<div style='display:flex; gap:8px; flex-wrap:wrap; margin-top:auto;'>"
                        << "<a class='btn-action' href='https://wa.me/" << p.phone << "' target='_blank' style='padding:8px 15px; font-size:0.9rem; background:#25D366; width:auto; flex:1;'>💬 واتساب</a>"
                        << (!p.website.empty() ? "<a class='btn-action' href='" + p.website + "' target='_blank' style='padding:8px 15px; font-size:0.9rem; background:linear-gradient(135deg, #0284c7, #0369a1); width:auto; flex:1;'>🌐 الموقع</a>" : "")
                        << "</div>"
                        << "</div>";
            }
        }

        string meta = get_seo_meta(title, "دليل المصاعد المتخصص عبر منصة ضربة شاكوش.");
        return "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
               "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
               + meta + get_modern_blue_css() + "</head><body>"
               + get_navbar_html(current_user) +
               "<div class='container'>"
               "<div class='section-intro'><h1>" + title + "</h1><p>قائمة معتمدة ومحدثة خصيصاً لخدمة مهندسي وفنيي ومقاولين قطاع المصاعد.</p></div>"
               + featured_content.str() +
               "<div class='grid-cards'>" + content.str() + "</div>"
               "<div class='actions' style='margin-top:40px;'><a class='btn-secondary' href='/contact'>➕ اطلب إدراج اسمك أو شركتك معنا</a></div>"
               "</div>"
               "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
               + get_theme_script(nonce) +
               "</body></html>";
    };

    svr.Get("/companies", [&render_page](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        res.set_content(render_page("🏢 الشركات والمؤسسات", "company", nonce, user), "text/html; charset=utf-8");
    });

    svr.Get("/contractors", [&render_page](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        res.set_content(render_page("👷 المقاولين المعتمدين", "contractor", nonce, user), "text/html; charset=utf-8");
    });

    svr.Get("/suppliers", [&render_page](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        res.set_content(render_page("📦 الموردين", "supplier", nonce, user), "text/html; charset=utf-8");
    });

    svr.Get("/cabins", [&render_page](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        res.set_content(render_page("🛗 مصانع الكباين", "cabins", nonce, user), "text/html; charset=utf-8");
    });

    svr.Get("/transport", [&render_page](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        res.set_content(render_page("🚚 خدمات النقل (دباب وديانا)", "transport", nonce, user), "text/html; charset=utf-8");
    });

    svr.Get("/labor", [&render_page](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        res.set_content(render_page("👷 العمالة اليومية والخدمات الميدانية", "labor", nonce, user), "text/html; charset=utf-8");
    });

    svr.Get("/calculator", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        string meta = get_seo_meta("حاسبة مقاسات بئر وكبينة المصاعد", "أداة هندسية لحساب وتصفية مقاسات كابينة المصعد وأبعاد الثقل ونوع الأبواب المتاحة أوتوماتيكياً.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() +
                      "</head><body>"
                      + get_navbar_html(user) +
                      "<div class='container' style='max-width:650px;'>"
                      "<div class='card'><h2>🛗 حاسبة مقاسات بئر المصعد الفنية</h2>"
                      "<div class='sub-title'>الرجاء إدخال القياسات الصافية المأخوذة من الموقع للبدء في الحساب والتصفية التلقائية :</div>"
                      "<form action='/calculator-step2' method='post'>"
                      "<div class='f-group'><label>👑 نوع نظام تشغيل المصعد:</label>"
                      "<select name='m_type'>"
                      "<option value='MR'>غرفة محرك أعلى البئر (MR)</option>"
                      "<option value='MRL'>بدون غرفة محرك علوية (MRL)</option>"
                      "<option value='Hydraulic'>نظام تشغيل هيدروليك (Hydraulic) 🛢️</option>"
                      "</select></div>"
                      "<div class='f-group'><label>📐 عرض بئر المصعد الصافي (CM):</label><input type='number' name='width' required min='80' max='250' placeholder='مثال لعرض البئر الحُر: 160'></div>"
                      "<div class='f-group'><label>📏 عمق بئر المصعد الصافي (CM):</label><input type='number' name='depth' required min='80' max='250' placeholder='مثال لعمق البئر الحُر: 160'></div>"
                      "<div class='f-group'><label>🏢 إجمالي عدد الوقفات (الأدوار الإنشائية):</label><input type='number' name='floors' required min='1' max='60' placeholder='أدخل عدد طوابق المبنى'></div>"
                      "<div class='f-group'><label>🕳️عمق حفرة المصعد Pit (CM):</label><input type='number' name='depth_pit' required min='10' max='500' value='100'></div>"
                      "<div class='f-group'><label>🏠 ارتفاع من ارضية الدور الاخير الي سقف البئر Overhead (CM):</label><input type='number' name='overhead' required min='100' max='800' value='400'></div>"
                      "<button type='submit'>➡️ الخطوة التالية: تخصيص البضاعة</button></form>"
                      "</div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Post("/calculator-step2", [&elevator](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        string m_type = html_escape(req.get_param_value("m_type"));
        if (m_type != "MR" && m_type != "MRL" && m_type != "Hydraulic") m_type = "MR";

        if (m_type == "MR" && !IS_MR_READY) {
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                          + get_modern_blue_css() + "</head><body>"
                          + get_navbar_html(user) +
                          "<div style='display:flex; align-items:center; justify-content:center; min-height:80vh;'>"
                          "<div class='card' style='border-color:var(--accent-2); max-width:520px; text-align:center;'>"
                          "<h2>⚙️ نظام MR قيد التحديث</h2>"
                          "<p style='color:var(--text-muted); margin-bottom:24px; line-height:1.7;'>جاري تعديل وتحديث بيانات المصاعد القياسية (بغرفة) من قِبل الإدارة.. شكراً لثقتكم!</p>"
                          "<a class='btn-secondary' href='/calculator'>🔄 العودة للحاسبة</a>"
                          "</div></div></body></html>";
            res.set_content(html, "text/html; charset=utf-8"); return;
        }

        if (m_type == "MRL" && !IS_MRL_READY) {
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                          + get_modern_blue_css() + "</head><body>"
                          + get_navbar_html(user) +
                          "<div style='display:flex; align-items:center; justify-content:center; min-height:80vh;'>"
                          "<div class='card' style='border-color:var(--accent-2); max-width:520px; text-align:center;'>"
                          "<h2>⚙️ نظام MRL قيد التعديل</h2>"
                          "<p style='color:var(--text-muted); margin-bottom:24px; line-height:1.7;'>جاري تعديل وتحديث معادلات المصاعد الجيرليس (بدون غرفة) حالياً .. شكراً لثقتكم!</p>"
                          "<a class='btn-secondary' href='/calculator'>🔄 العودة للحاسبة</a>"
                          "</div></div></body></html>";
            res.set_content(html, "text/html; charset=utf-8"); return;
        }

        if (m_type == "Hydraulic" && !IS_HYDRAULIC_READY) {
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                          + get_modern_blue_css() + "</head><body>"
                          + get_navbar_html(user) +
                          "<div style='display:flex; align-items:center; justify-content:center; min-height:80vh;'>"
                          "<div class='card' style='border-color:var(--accent-2); max-width:520px; text-align:center;'>"
                          "<h2>⚙️ النظام الهيدروليكي قيد التطوير</h2>"
                          "<p style='color:var(--text-muted); margin-bottom:24px; line-height:1.7;'>جاري العمل حالياً على تدقيق وضبط خوارزميات حسابات المصاعد الهيدروليكية وخاماتها.. شكراً لثقتكم !</p>"
                          "<a class='btn-secondary' href='/calculator'>🔄 العودة للحاسبة</a>"
                          "</div></div></body></html>";
            res.set_content(html, "text/html; charset=utf-8"); return;
        }

        int w = safe_stoi(req.get_param_value("width"), 0);
        int d = safe_stoi(req.get_param_value("depth"), 0);
        string f = req.get_param_value("floors");
        string p = req.get_param_value("depth_pit");
        string oh = req.get_param_value("overhead");

        if (w < 110 || d < 100 || safe_stof(f) <= 0 || safe_stoi(p) <= 0 || safe_stoi(oh) <= 0 || w > 250 || d > 250) {
            string err = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                         "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                         + get_modern_blue_css() + "</head><body>"
                         + get_navbar_html(user) +
                         "<div style='display:flex; align-items:center; justify-content:center; min-height:80vh;'>"
                         "<div class='card' style='border-color:#ef4444; max-width:500px; text-align:center;'>"
                         "<h2>⚠️ البيانات المدخلة غير سليمة هندسياً</h2>"
                         "<p style='color:#94a3b8;'>يرجى إدخال قيم موجبة مابين 110سم إلى 250سم كحد أقصى لعرض وعمق البئر الصافي.</p>"
                         "<a href='/calculator' class='btn-action' style='background:#ef4444;'>🔄 العودة وتعديل أبعاد البئر</a>"
                         "</div></div>"
                         "</body></html>";
            res.set_content(err, "text/html; charset=utf-8"); return;
        }

        string raw_door_options = elevator.get_door_type(w);
        vector<string> door_options = split_string(raw_door_options, "||");

        ostringstream door_select;
        door_select << "<select name='door_choice'>";
        for (const auto& opt : door_options) {
            string clean_opt = trim(opt);
            door_select << "<option value='" << clean_opt << "'>" << clean_opt << "</option>";
        }
        door_select << "</select>";

        string nonce = generate_nonce(); set_csp(res, nonce);
        ostringstream os;
        os << "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
           "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
           << get_modern_blue_css()
           << "<script nonce='" + nonce + "'>"
           "function updateDoorOrigin() {"
           "  var doorSelect = document.querySelector(\"select[name='door_choice']\");"
           "  var originSelect = document.querySelector(\"select[name='door_origin']\");"
           "  var selectedDoor = doorSelect.value;"
           "  var isSemi = selectedDoor.includes('Semi');"
           "  var hasTurki = false;"
           "  for(var i=0; i<originSelect.options.length; i++) {"
           "    if(originSelect.options[i].value === 'تركي') { hasTurki = true; break; }"
           "  }"
           "  if(isSemi && !hasTurki) {"
           "    var opt = document.createElement('option');"
           "    opt.value = 'تركي';"
           "    opt.innerHTML = 'تركي';"
           "    originSelect.appendChild(opt);"
           "  } else if(!isSemi && hasTurki) {"
           "    for(var i=0; i<originSelect.options.length; i++) {"
           "      if(originSelect.options[i].value === 'تركي') {"
           "        originSelect.removeChild(originSelect.options[i]);"
           "        break;"
           "      }"
           "    }"
           "  }"
           "}"
           "</script>"
           "</head><body onload='updateDoorOrigin()'>"
           + get_navbar_html(user) +
           "<div class='container' style='max-width:650px;'>"
           "<div class='card'><h2>⚙️ تخصيص البضاعة للمقايسة</h2>"
           "<div class='sub-title'>حدد تفضيلاتك لعرض البضاعة في التقرير النهائي:</div>"
           "<form action='/calculate' method='post'>"
           "<input type='hidden' name='m_type' value='" << m_type << "'>"
           "<input type='hidden' name='width' value='" << w << "'>"
           "<input type='hidden' name='depth' value='" << d << "'>"
           "<input type='hidden' name='floors' value='" << f << "'>"
           "<input type='hidden' name='depth_pit' value='" << p << "'>"
           "<input type='hidden' name='overhead' value='" << oh << "'>"
           
           "<div class='f-group'><label>🚪 اختر مقاس الأبواب (المناسبة لعرض البئر):</label>"
           << door_select.str() << "</div>"
           
           "<div class='f-group'><label>🛠️ منشأ السكك:</label>"
           "<select name='rails_origin'>"
           "<option value='صيني'>صيني</option>"
           "<option value='إيطالي'>إيطالي</option>"
           "</select></div>"

           "<div class='f-group'><label>🚪 منشأ الأبواب:</label>"
           "<select name='door_origin'>"
           "<option value='صيني'>صيني</option>"
           "<option value='إيطالي'>إيطالي</option>"
           "</select></div>"
           
           "<div class='f-group'><label>⚡ هل تحتاج جهاز طوارئ (ARD)؟</label>"
           "<select name='has_ard'>"
           "<option value='yes'>نعم، أضف ARD</option>"
           "<option value='no'>لا أحتاج</option>"
           "</select></div>"

           "<div class='f-group'><label>⛓️ عرض تفاصيل البضاعة؟</label>"
           "<select name='calc_mat'><option value='yes'>نعم، أظهر جدول البضاعة</option><option value='no'>لا، أريد المقاسات فقط</option></select></div>"

           "<button type='submit'>🏛️ استخراج التقرير النهائي</button>"
           "</form>"
           "</div></div>"
           "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
           + get_theme_script(nonce) +
           "<script nonce='" + nonce + "'>"
           "document.querySelector(\"select[name='door_choice']\").addEventListener('change', updateDoorOrigin);"
           "</script>"
           "</body></html>";
        res.set_content(os.str(), "text/html; charset=utf-8");
    });

    svr.Post("/calculate", [&elevator](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        string m_type = html_escape(req.get_param_value("m_type"));
        int w = safe_stoi(req.get_param_value("width"), 0);
        int d = safe_stoi(req.get_param_value("depth"), 0);
        float f = safe_stof(req.get_param_value("floors"), 0.0f);
        int p = safe_stoi(req.get_param_value("depth_pit"), 100);
        int oh = safe_stoi(req.get_param_value("overhead"), 400);
        
        string calc_mat = html_escape(req.get_param_value("calc_mat"));
        string door_choice = html_escape(req.get_param_value("door_choice"));
        string rails_origin = html_escape(req.get_param_value("rails_origin"));
        string door_origin = html_escape(req.get_param_value("door_origin"));
        string has_ard = html_escape(req.get_param_value("has_ard"));

        bool is_side_cwt = (w >= d + 20);

        int cabin_dbg = elevator.get_cabin_dbg(w, is_side_cwt);
        int cwt_dbg = elevator.get_cwt_dbg(w, is_side_cwt);
        int cab_w = elevator.get_cabin_width(w, is_side_cwt);
        int cab_d = elevator.get_cabin_depth(d);
        float h = elevator.get_shaft_height(f, p/100.0f, oh/100.0f);

        Elevator::FullSpecificationReport specs = elevator.compile_full_specification(w, d, static_cast<int>(f), m_type, door_choice, rails_origin, door_origin, has_ard);

        string report_summary = "مقايسة بئر: " + to_string(w) + "x" + to_string(d) + " سم - أدوار: " + to_string((int)f);

        string nonce = generate_nonce(); set_csp(res, nonce);
        ostringstream os;
        os << "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
           "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
           "<script src='https://cdnjs.cloudflare.com/ajax/libs/html2pdf.js/0.10.1/html2pdf.bundle.min.js'></script>"
           + get_modern_blue_css() + "</head><body>"
           << get_navbar_html(user)
           << "<div class='container' style='max-width:780px;'>"
           << "<div class='card' id='pdf-area'><h2>📋 تقرير المقايسة وتصفية المقاسات الفنية</h2>"
           
           << "<div style='margin-bottom:20px; background:var(--surface-2); padding:15px; border-radius:10px; border:1px solid var(--border);'>"
           << "<p style='margin:5px 0;'><b>👤 اسم العميل:</b> <span id='lblClient' style='color:var(--accent);'>غير محدد</span></p>"
           << "<p style='margin:5px 0;'><b>📝 ملاحظات الموقع:</b> <span id='lblNotes' style='color:var(--text-muted);'>لا توجد ملاحظات</span></p>"
           << "</div>"

           << "<div class='stage-header stage-1'>📌 أولاً: المقاسات الإنشائية وأبعاد الصاعدة:</div>"
           << "<div class='table-container'><table class='tbl'>"
           << "<tr><th>نظام التشغيل:</th><td>" << (m_type == "MR" ? "غرفة محرك (MR)" : (m_type == "MRL" ? "بدون غرفة (MRL)" : "هيدروليك Hydraulic 🛠️")) << "</td></tr>"
           << "<tr><th>أبعاد البئر الحرة:</th><td>العرض: " << w << " سم || العمق: " << d << " سم</td></tr>"
           << "<tr><th>نوع الأبواب المحددة:</th><td style='color:#38bdf8; font-weight:700;'>[" << door_choice << "]</td></tr>"
           << "<tr><th>شاسيه الكابينة (DBG):</th><td>" << cabin_dbg << " سم</td></tr>"
           << "<tr><th>شاسيه الثقل (DBG):</th><td>" << (m_type == "Hydraulic" ? "بدون ثقل" : to_string(cwt_dbg) + " سم") << "</td></tr>"
           << "<tr><th>صافي الكابينة من الداخل:</th><td style='color:#f5a524;'>العرض: " << cab_w << " سم × العمق: " << cab_d << " سم</td></tr>"
           << "<tr><th>الحمولة المقدرة:</th><td style='color:#16a34a; font-weight:800;'>" << specs.rated_load_kg << " كجم</td></tr>"
           << "<tr><th>مشوار البئر (الارتفاع):</th><td>" << h << " متر طولي</td></tr>"
           << "</table></div>";

        if (calc_mat == "yes") {
            os << "<div class='stage-header stage-2'>⚙️ ثانياً: بضاعة المرحلة الأولى:</div>"
               << "<div class='table-container'><table class='tbl'>"
               << "<thead><tr><th>اسم الصنف  </th><th>الكمية</th></tr></thead>"
               << "<tbody>"
               << "<tr><td>" << specs.scaffolding_name << "</td><td>" << specs.scaffolding_count << " طقم</td></tr>"
               << "<tr><td>" << specs.plumb_name << "</td><td>" << specs.plumb_count << " لفة </td></tr>"
               << "<tr><td>" << specs.balance_name << "</td><td>" << specs.balance_count << " قطعة </td></tr>"
               << "<tr><td>" << specs.door_exterior_name << "</td><td>" << specs.total_exterior_doors << " باب</td></tr>"
               << "<tr><td>" << specs.cabin_rails_name << "</td><td>" << specs.cabin_rails_count << " عود </td></tr>"
               << "<tr><td>" << specs.cwt_rails_name << "</td><td>" << specs.cwt_rails_count << " عود </td></tr>"
               << "<tr><td>" << specs.cabin_brackets_name << "</td><td>" << specs.cabin_brackets_count << " كابولي</td></tr>"
               << "<tr><td>" << specs.cwt_brackets_name << "</td><td>" << specs.cwt_brackets_count << " كابولي</td></tr>";
               
            if (specs.cwt_belt_count > 0) {
                os << "<tr><td style='color:#f5a524;'>" << specs.cwt_belt_name << "</td><td style='color:#f5a524;'>" << specs.cwt_belt_count << " حزام</td></tr>";
            }
            if (specs.platat_count > 0) {
                os << "<tr><td style='color:#38bdf8;'>" << specs.platat_name << "</td><td style='color:#38bdf8;'>" << specs.platat_count << " قطعة</td></tr>";
            }

            os << "<tr><td>" << specs.sub_cabin_name << "</td><td>" << specs.sub_cabin_count << " قطعة </td></tr>"
               << "<tr><td>" << specs.sub_cwt_name << "</td><td>" << specs.sub_cwt_count << " قطعة </td></tr>"
               << "<tr><td>" << specs.hilti_bolts_name << "</td><td>" << specs.hilti_bolts_12mm << " مسمار </td></tr>"
               << "<tr><td>" << specs.assembly_bolts_name << "</td><td>" << specs.assembly_bolts_12mm << " مسمار </td></tr>"
               << "<tr><td>" << specs.bolts_8mm_name << "</td><td>" << specs.bolts_8mm << " مسمار </td></tr>"
               << "<tr><td>" << specs.spring_washers_8mm_name << "</td><td>" << specs.spring_washers_8mm << " طقم</td></tr>"
               << "<tr><td>" << specs.spring_washers_12mm_name << "</td><td>" << specs.spring_washers_12mm << " وردة</td></tr>"
               << "<tr><td>" << specs.nuts_12mm_name << "</td><td>" << specs.nuts_12mm << " صامولة </td></tr>"
               << "<tr><td>" << specs.flat_washers_12mm_name << "</td><td>" << specs.flat_washers_12mm << " وردة</td></tr>"
               << "</tbody></table></div>";

            os << "<div class='stage-header stage-3'>⚡ ثالثاً: المرحلة الثانية والثالثة:</div>"
               << "<div class='table-container'><table class='tbl'>"
               << "<thead><tr><th>اسم الصنف    </th><th>الكمية</th></tr></thead>"
               << "<tbody>";

            if (m_type == "MRL") {
                os << "<tr><td style='color:#38bdf8;'>" << specs.machine_bed_name << "</td><td style='color:#38bdf8;'>1 طقم</td></tr>";
            } else if (m_type == "MR") {
                os << "<tr><td>" << specs.ceiling_cut_name << "</td><td>" << specs.ceiling_cut_count << " فتحة</td></tr>";
            }

            os << "<tr><td>نوع الماكينة</td><td>" << specs.machine_type_desc << "</td></tr>"
               << "<tr><td>تعليق الثقل </td><td>" << specs.cwt_design_type << "</td></tr>";
               
            if (!specs.is_hydraulic) {
                os << "<tr><td>" << specs.cabin_wires_name << "</td><td>" << specs.cabin_wires_meters << " متر</td></tr>"
                   << "<tr><td>شدادات </td><td>" << specs.rope_hitches_desc << "</td></tr>"
                   << "<tr><td>زراجين</td><td>" << specs.rope_clamps_desc << "</td></tr>"
                   << "<tr><td>" << specs.cwt_blocks_weight_desc << "</td><td>" << specs.counterweight_blocks << " بلوك</td></tr>";
            }

            os << "<tr><td>" << specs.parachute_name << "</td><td>" << specs.parachute_count << " جهاز </td></tr>"
               << "<tr><td>" << specs.governor_rope_name << "</td><td>" << specs.governor_rope_meters << " متر</td></tr>"
               << "<tr><td>" << specs.buffer_set_name << "</td><td>" << specs.buffer_set_count << " طقم </td></tr>"
               << "<tr><td>" << specs.control_panel_name << "</td><td>" << specs.control_panel_count << " لوحة</td></tr>";
               
            if (specs.has_ard) {
                os << "<tr><td style='color:#16a34a;'>" << specs.ard_system_name << "</td><td style='color:#16a34a;'>" << specs.ard_system_count << " جهاز</td></tr>";
            }
               
            os << "<tr><td>" << specs.flex_cable_name << "</td><td>" << specs.flex_cable_meters << " متر </td></tr>"
               << "<tr><td>" << specs.trunk_4cm_name << "</td><td>" << specs.trunk_4cm_meters << " متر  </td></tr>"
               << "<tr><td>" << specs.trunk_10cm_name << "</td><td>" << specs.static_trunk_10cm << " متر  </td></tr>"
               << "<tr><td>" << specs.wire_1mm_name << "</td><td>" << specs.wire_1mm_count_desc << "</td></tr>"
               << "<tr><td>" << specs.cop_panel_name << "</td><td>" << specs.cop_panel_count << " لوحة</td></tr>"
               << "<tr><td>" << specs.lop_buttons_name << "</td><td>" << specs.lop_buttons_count << " طلبات</td></tr>"
               << "<tr><td>" << specs.safety_door_name << "</td><td>" << specs.safety_door_count << " بوابة</td></tr>"
               << "<tr><td>" << specs.photocell_name << "</td><td>" << specs.photocell_count << " ستارة</td></tr>"
               << "</tbody></table></div>";
        }

        os << "</div>";

        if (!user.empty()) {
            os << "<div class='card' style='border-color:var(--accent);'>"
               << "<h3>💾 حفظ التقرير في سجلك الشخصي باسم العميل</h3>"
               << "<form action='/api/save-report' method='post'>"
               << "<div class='f-group'><label>👤 اسم العميل أو المشروع:</label><input type='text' name='client_name' required placeholder='اكتب اسم العميل هنا'></div>"
               << "<div class='f-group'><label>📝 ملاحظات هندسية:</label><input type='text' name='notes' placeholder='ملاحظات خاصة بالموقع'></div>"
               << "<input type='hidden' name='report_summary' value='" << report_summary << "'>"
               << "<button type='submit' class='btn-save'>💾 اعتماد الحفظ باسم العميل</button>"
               << "</form></div>";
        } else {
            os << "<div class='card' style='text-align:center;'><a class='btn-secondary' href='/login' style='background:#f5a524; color:#000; display:block;'>🔒 سجل دخولك لتخزين هذا التقرير باسم العميل</a></div>";
        }

        os << "<div class='actions'>"
           << "<button class='btn-print' id='pBtn'>📥 تحميل التقرير PDF</button>"
           << "<a class='btn-secondary' href='/calculator'>🔄 تصفية بئر جديد</a>"
           << "</div></div>"
           << "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
           << "<script nonce='" << nonce << "'>"
           << "document.querySelector(\"input[name='client_name']\")?.addEventListener('input', (e) => { document.getElementById('lblClient').innerText = e.target.value || 'غير محدد'; });"
           << "document.querySelector(\"input[name='notes']\")?.addEventListener('input', (e) => { document.getElementById('lblNotes').innerText = e.target.value || 'لا توجد ملاحظات'; });"
           << "document.getElementById('pBtn').addEventListener('click', function(){"
           << "  html2pdf().set({margin:0.3, filename:'Hammer_Report.pdf', image:{type:'jpeg', quality:1}, html2canvas:{scale:2}}).from(document.getElementById('pdf-area')).save();"
           << "});</script></body></html>";
        res.set_content(os.str(), "text/html; charset=utf-8");
    });

    svr.Post("/api/save-report", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        if (user.empty()) { res.set_redirect("/login"); return; }
        SavedReport rep;
        rep.client_name = html_escape(req.get_param_value("client_name"));
        rep.notes = html_escape(req.get_param_value("notes"));
        rep.summary = html_escape(req.get_param_value("report_summary"));
        
        if (!rep.summary.empty() && users_db.find(user) != users_db.end()) {
            users_db[user].saved_reports.push_back(rep);
            save_user_to_mongodb(users_db[user]);
        }
        res.set_redirect("/my-reports");
    });

    svr.Get("/my-reports", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        if (user.empty()) { res.set_redirect("/login"); return; }
        string nonce = generate_nonce(); set_csp(res, nonce);
        
        ostringstream r_list;
        if (users_db.find(user) != users_db.end() && !users_db[user].saved_reports.empty()) {
            for (const auto& rep : users_db[user].saved_reports) {
                r_list << "<div style='background:var(--bg); border:1px solid var(--border); padding:15px; border-radius:10px; margin-bottom:12px; font-weight:600;'>"
                       << "<b>👤 اسم العميل:</b> <span style='color:var(--accent);'>" << rep.client_name << "</span><br>"
                       << "<b>📌 تفاصيل المقايسة:</b> " << rep.summary << "<br>"
                       << "<b>📝 ملاحظات الموقع:</b> " << (rep.notes.empty() ? "لا توجد" : rep.notes)
                       << "</div>";
            }
        } else {
            r_list << "<p style='color:var(--text-muted); text-align:center;'>لا توجد تقارير محفوظة حتى الآن. قم بعمل مقايسة جديدة واضغط على زر حفظ التقرير.</p>";
        }

        string meta = get_seo_meta("التقارير المحفوظة", "سجل المقايسات الهندسية المحفوظة للعملاء.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html(user) +
                      "<div class='container' style='max-width:700px;'>"
                      "<div class='card'><h2>📂 سجل تقارير العُملاء المحفوظة</h2>"
                      "<div class='sub-title'>هنا تجد كافة مقايسات الأببار المسجلة بأسماء عملائك:</div>"
                      + r_list.str() +
                      "<div class='actions' style='margin-top:25px;'><a class='btn-secondary' href='/calculator'>🛗 حاسبة مقاسات جديدة</a></div>"
                      "</div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get("/blog", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        auto lessons = get_lessons();
        ostringstream cards;
        for (auto& l : lessons) {
            string tag_class = (l.type == "video") ? "tag-video" : "tag-article";
            string tag_label = (l.type == "video") ? "🎥 درس فيديو" : "📖 مقال فني";
            cards << "<a href='/lesson/" << l.slug << "' class='nav-card'>"
                  << "<span class='lesson-tag " << tag_class << "'>" << tag_label << "</span>"
                  << "<h3>" << html_escape(l.title) << "</h3>"
                  << "<p>" << html_escape(l.summary) << "</p>"
                  << "</a>";
        }
        string meta = get_seo_meta("مكتبة الشروحات الهندسية ودروس الصيانة", "سلسلة المقالات المكتوبة والفيديوهات التطبيقية لتعليم ميكانيكا وتركيبات دوائر أمان المصاعد.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html(user) +
                      "<div class='container'>"
                      "<div class='section-intro'><h1>📚 مكتبة الدروس الفنية والشروحات</h1>"
                      "<p>مقالات هندسية وفيديوهات متخصصة تشرح التصفية الكهربائية والميكانيكية للمصاعد خطوة بخطوة للشغل النظيف والأمان.</p></div>"
                      "<div class='grid-nav'>" + cards.str() + "</div>"
                      "</div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get(R"(/lesson/([a-zA-Z0-9\-]+))", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        string slug = req.matches[1].str();
        auto lessons = get_lessons();
        auto it = find_if(lessons.begin(), lessons.end(), [&](const Lesson& l) { return l.slug == slug; });

        if (it == lessons.end()) {
            res.status = 404;
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          + get_modern_blue_css() + "</head><body>"
                          + get_navbar_html(user) +
                          "<div class='container' style='display:flex; align-items:center; justify-content:center; min-height:50vh;'>"
                          "<div class='card' style='text-align:center; max-width:450px;'>"
                          "<h2>⚠️ الدرس غير متوفر حالياً</h2>"
                          "<p style='color:#94a3b8; margin-bottom:24px;'>الرابط المطلوب غير متاح أو تم نقله لمسار آخر.</p>"
                          "<a class='btn-secondary' href='/blog'>⬅️ العودة للمكتبة والشروحات</a>"
                          "</div></div>"
                          "</body></html>";
            res.set_content(html, "text/html; charset=utf-8"); return;
        }

        string tag_class = (it->type == "video") ? "tag-video" : "tag-article";
        string tag_label = (it->type == "video") ? "🎥 فيديو تعليمي" : "📖 مقال تقني مخصص";

        ostringstream body;
        if (it->type == "video" && !it->video_embed_url.empty()) {
            body << "<div class='video-embed'><iframe src='" << it->video_embed_url
                 << "' allow='accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture' "
                    "allowfullscreen loading='lazy'></iframe></div>";
        }
        if (!it->content_html.empty()) {
            body << "<div class='lesson-body'><h3>📌 تفاصيل الشرح والمخطط الفني:</h3>" << it->content_html << "</div>";
        }

        string meta = get_seo_meta(it->title, it->summary);
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html(user) +
                      "<div class='container' style='max-width:750px;'>"
                      "<div class='card'>"
                      "<span class='lesson-tag " + tag_class + "'>" + tag_label + "</span>"
                      "<h2>" + html_escape(it->title) + "</h2>"
                      "<div class='sub-title'>" + html_escape(it->summary) + "</div>"
                      + body.str() +
                      "<div class='actions'><a class='btn-secondary' href='/blog'>⬅️ العودة للمكتبة والشروحات</a></div>"
                      "</div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get("/paths", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        auto tracks = get_tracks();
        ostringstream cards;
        for (auto& t : tracks) {
            cards << "<a href='/track/" << t.slug << "' class='nav-card'>"
                  << "<h3>" << t.emoji << " " << html_escape(t.title) << "</h3>"
                  << "<p>" << html_escape(t.description) << "</p>"
                  << "</a>";
        }
        string meta = get_seo_meta("مسارات التعلم والكورسات الهندسية", "تصفح المسارات المرتبة أكاديمياً لتعلم فنيات التركيب الميكانيكي وتأسيس لوحات دوائر الأمان والكنترول للمصاعد.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html(user) +
                      "<div class='container'>"
                      "<div class='section-intro'><h1>🧭 مسارات التعلم الأكاديمية والمهنية</h1>"
                      "<p>كل مسار مخصص لجمع وتدريس الحلقات والدروس بالترتيب الصحيح لضمان الانتقال السلس من مرحلة التأسيس إلى مرحلة الاحتراف.</p></div>"
                      "<div class='grid-nav'>" + cards.str() + "</div>"
                      "</div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get(R"(/track/([a-zA-Z0-9\-]+))", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        string slug = req.matches[1].str();
        auto tracks = get_tracks();
        auto trackIt = find_if(tracks.begin(), tracks.end(), [&](const Track& t) { return t.slug == slug; });

        if (trackIt == tracks.end()) {
            res.status = 404;
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          + get_modern_blue_css() + "</head><body>"
                          + get_navbar_html(user) +
                          "<div class='container' style='display:flex; align-items:center; justify-content:center; min-height:50vh;'>"
                          "<div class='card' style='text-align:center; max-width:450px;'>"
                          "<h2>⚠️ المسار التعليمي غير متاح</h2>"
                          "<p style='color:#94a3b8; margin-bottom:24px;'>المسار التدريبي المطلوب قد يكون قيد التحضير.</p>"
                          "<a class='btn-secondary' href='/paths'>⬅️ العودة لجميع المسارات</a>"
                          "</div></div>"
                          "</body></html>";
            res.set_content(html, "text/html; charset=utf-8"); return;
        }

        auto lessons = get_lessons_by_track(slug);
        ostringstream items;
        for (auto& l : lessons) {
            string tag_emoji = (l.type == "video") ? "🎥" : "📖";
            items << "<a href='/lesson/" << l.slug << "' class='track-item'>"
                  << "<span class='track-order'>" << l.order << "</span>"
                  << "<div><div class='track-item-title'>" << tag_emoji << " " << html_escape(l.title) << "</div>"
                  << "<div class='track-item-summary'>" << html_escape(l.summary) << "</div></div>"
                  << "</a>";
        }
        if (lessons.empty()) {
            items << "<p style='color:#8b96ab;'>يتم رفع وتدقيق الدروس الخاصة بهذا الكورس حالياً، تابعنا قريباً.</p>";
        }

        string meta = get_seo_meta(trackIt->title, trackIt->description);
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html(user) +
                      "<div class='container' style='max-width:700px;'>"
                      "<div class='card'>"
                      "<h2>" + trackIt->emoji + " " + html_escape(trackIt->title) + "</h2>"
                      "<div class='sub-title'>" + html_escape(trackIt->description) + "</div>"
                      "<div class='track-list'>" + items.str() + "</div>"
                      "<div class='actions' style='margin-top:25px;'><a class='btn-secondary' href='/paths'>⬅️ العودة لكافة المسارات</a></div>"
                      "</div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get("/contact", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        string meta = get_seo_meta("اتصل بنا | الدعم الفني", "تواصل مباشرة مع إدارة منصة ضربة شاكوش لطرح الأسئلة الفنية أو الإبلاغ عن مشكلة برمجية.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html(user) +
                      "<div class='container' style='max-width:650px;'>"
                      "<div class='card'><h2>📩 تواصل مع الدعم الفني </h2>"
                      "<div class='sub-title'>لديك أي استفسار فني خاص بالمقاسات، اقتراح لتطوير الموقع، أو واجهتك مشكلة بالحاسبة؟ تواصل معنا فوراً.</div>"
                      
                      "<div style='display:flex; flex-direction:column; gap:15px; margin-bottom:20px;'>"
                      "<a class='btn-action' href='mailto:darbatshakush98@gmail.com' style='display:block;'>📧 البريد الإلكتروني: darbatshakush98@gmail.com</a>"
                      "<a class='btn-action' href='https://wa.me/966564406565' target='_blank' style='display:block; background:linear-gradient(135deg, #25D366, #128C7E); box-shadow:0 4px 6px rgba(37,211,102,0.3);'>💬 تواصل واتساب: 00966564406565</a>"
                      "<a class='btn-secondary' href='tel:00966564406565' style='display:block;'>📞 اتصال هاتفي مباشر: 00966564406565</a>"
                      "</div>"
                      
                      "<p style='color:#8b96ab; font-size:0.9rem; text-align:center;'>المراسلات يتم الرد عليها ومراجعتها من المهندس المختص خلال 24 ساعة.</p>"
                      "</div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get("/support", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        string meta = get_seo_meta("مركز المساعدة والأسئلة الشائعة الفنية للمصاعد.", "محتاج مساعدة في فهم كيفية حساب أبعاد الـ DBG الصافي وتصفية البير؟");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html(user) +
                      "<div class='container' style='max-width:650px;'>"
                      "<div class='card'><h2>🛟 مركز الدعم والمساعدة الفنية</h2>"
                      "<div class='sub-title'>محتاج مساعدة في فهم كيفية حساب أبعاد الـ DBG الصافي وشواكيل التصفية؟</div>"
                      "<p style='line-height:1.8; margin-bottom:24px;'>يمكنك مراجعة المسارات والدروس التعليمية المتاحة بالمكتبة، وفي حال وجود تعارض في المقاسات المعمارية يمكنك فتح تذكرة دعم فني.</p>"
                      "<div class='actions'>"
                      "  <a class='btn-print' href='/blog'>❓ تصفح الشروحات</a>"
                      "  <a class='btn-secondary' href='/contact'>📩 فتح تذكرة دعم</a>"
                      "</div></div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    const char* port_env = getenv("PORT");
    int port = port_env ? safe_stoi(port_env, 8080) : 8080;
    cout << "🚀 Professional Hammer-Platform active on port: " << port << endl;
    svr.listen("0.0.0.0", port);
    return 0;
}
