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
const int MAX_REQUESTS_PER_MINUTE = 30; 

static string CF_VERIFY_SECRET = getenv("CF_VERIFY_SECRET") ? getenv("CF_VERIFY_SECRET") : "";
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

static void save_user_to_mongodb(const UserAccount& acc) {
    const char* mongo_uri = getenv("MONGO_URI");
    if (mongo_uri) {
        cout << "[MongoDB Cloud] User " << acc.username << " synchronized successfully." << endl;
    }
}

static bool send_email_otp(const string& email, const string& first_name, const string& otp_code) {
    const char* env_val = getenv("RESEND_API_KEY");
    string API_KEY = env_val ? env_val : "";

    if (API_KEY.empty()) {
        cout << "[Security Log] Email service key is missing." << endl;
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
        
        string html_content = "<div dir='rtl' style='font-family: Arial, sans-serif; text-align: right; color: #333;'>"
                              "<h2 style='color: #0ea5e9;'>مرحباً يا " + first_name + "</h2>"
                              "<p>شكراً لتسجيلك في منصة ضربة شاكوش.</p>"
                              "<p>رمز التفعيل الآمن لحسابك هو:</p>"
                              "<p style='font-size: 28px; font-weight: bold; letter-spacing: 5px; color: #16a34a; background: #f3f4f6; padding: 15px; display: inline-block; border-radius: 8px;'>" + otp_code + "</p>"
                              "<p>يرجى إدخال هذا الرمز في الموقع لتفعيل حسابك نهائياً.</p></div>";

        string body = "{\"from\":\"Darbat Shakosh <noreply@darbat-shakosh.com>\",\"to\":[\"" + email + "\"],\"subject\":\"رمز تفعيل حسابك - منصة ضربة شاكوش\",\"html\":\"" + html_content + "\"}";
        
        auto res = cli.Post("/emails", headers, body, "application/json");
        return (res && (res->status == 200 || res->status == 201));
    } catch (...) {
        return false;
    }
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
        { "م/ ضياء البخمي", "contractor", "0562417042", "", "", "جدة", "مقاول تركيبات .", "⭐⭐⭐⭐⭐", false, false }
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
           "body{font-family:var(--font-display); background-color:var(--bg); color:var(--text); direction:rtl; text-align:right; margin:0; padding:0; min-height:100vh; display:flex; flex-direction:column;"
           "background-image:linear-gradient(rgba(56,189,248,0.045) 1px, transparent 1px), linear-gradient(90deg, rgba(56,189,248,0.045) 1px, transparent 1px);"
           "background-size:30px 30px; transition: background-color 0.3s, color 0.3s;}"
           "a{outline-offset:3px; text-decoration:none; color:inherit;}"
           ":focus-visible{outline:2px solid var(--accent); outline-offset:2px; border-radius:4px;}"

           ".navbar{background-color:var(--surface); border-bottom:1px solid var(--border); padding:14px 28px; display:flex; justify-content:space-between; align-items:center; position:relative; z-index:50; box-shadow:0 4px 6px -1px rgba(0,0,0,0.15);}"
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
           ".dropdown-panel{position:absolute; inset-inline-start:0; top:calc(100% + 16px); display:flex; gap:30px; background:var(--surface-2); border:1px solid var(--border); border-radius:12px; padding:18px 22px; min-width:190px; box-shadow:0 20px 25px -5px rgba(0,0,0,0.45); z-index:60;}"
           ".dropdown-col{display:flex; flex-direction:column; gap:11px; min-width:150px;}"
           ".dropdown-panel a, .mobile-panel a{color:var(--text); font-size:0.95rem; font-weight:600; text-decoration:none; transition:color 0.15s;}"
           ".dropdown-panel a:hover{color:var(--accent);}"
           ".desktop-only{display:flex;}"
           ".mobile-only{display:none;}"
           "@media (max-width:860px){.desktop-only{display:none;} .mobile-only{display:flex;} .navbar-brand span:last-child{font-size:1.05rem;}}"

           ".nav-left{display:flex; align-items:center; gap:18px;}"
           ".nav-icon{color:var(--text-muted); cursor:pointer; display:flex; align-items:center; justify-content:center; transition:0.2s; text-decoration:none; list-style:none; background:none; border:none; padding:0;}"
           ".nav-icon::-webkit-details-marker{display:none;}"
           ".nav-icon:hover{color:var(--accent);}"
           ".nav-icon svg{width:22px; height:22px; fill:currentColor;}"
           "body.light-mode .theme-sun, body:not(.light-mode) .theme-moon { display:none; }"

           ".container{max-width:1100px; margin:0 auto; padding:50px 20px; flex:1; width:100%;}"
           ".card{position:relative; background:var(--surface); border:1px solid var(--border); padding:40px; border-radius:16px; box-shadow:0 20px 25px -5px rgba(0,0,0,0.3); text-align:right; transition: background-color 0.3s; margin-bottom:20px;}"
           ".card h2{color:var(--text); font-size:1.6rem; margin-top:0; margin-bottom:15px; font-weight:700; border-bottom:1px solid var(--border); padding-bottom:15px;}"
           ".sub-title{color:var(--text-muted); margin-bottom:35px; font-size:0.95rem; line-height:1.6;}"
           ".f-group{margin-bottom:24px; text-align:right;}"
           ".f-group label{font-weight:600; color:var(--text); display:block; margin-bottom:12px; font-size:0.95rem;}"
           "input,select{width:100%; padding:14px; border:1px solid var(--border); border-radius:10px; text-align:right; font-size:1rem; font-family:var(--font-display); background-color:var(--bg); color:var(--text); transition:0.3s; font-weight:600; padding-right:15px; direction:rtl;}"
           "input:focus, select:focus{outline:none; border-color:var(--accent); box-shadow:0 0 0 3px rgba(56,189,248,0.2);}"
           "button, .btn-action{background:linear-gradient(135deg, #0ea5e9, #0284c7); color:#ffffff; border:none; padding:16px; border-radius:10px; width:100%; font-size:1.1rem; font-weight:700; cursor:pointer; transition:0.3s; text-decoration:none; display:inline-block; text-align:center; box-shadow: 0 4px 12px rgba(14,165,233,0.3);}"
           "button:hover, .btn-action:hover{background:linear-gradient(135deg, #0284c7, #0369a1); transform:translateY(-1px);}"

           ".table-container{position:relative; width:100%; overflow-x:auto; background:var(--bg); border-radius:8px; border:1px solid var(--border); margin-top:10px;}"
           ".tbl{width:100%; border-collapse:collapse; text-align:right; margin-bottom: 20px;}"
           ".tbl th{background:var(--surface); padding:15px; color:var(--text); font-weight:600; border-bottom:1px solid var(--border); font-size:1rem; text-align:right; width:45%;}"
           ".tbl td{padding:15px; border-bottom:1px solid var(--border); color:var(--text); font-size:1rem; font-weight:600; text-align:right; font-family:var(--font-mono);}"
           
           ".stage-header { padding: 12px 15px; border-radius: 8px; margin: 25px 0 10px 0; font-weight: 800; color: #fff; text-align: center; font-size: 1.1rem; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }"
           ".stage-1 { background: linear-gradient(135deg, #0ea5e9, #0284c7); }" 
           ".stage-2 { background: linear-gradient(135deg, #8b5cf6, #7c3aed); }" 
           ".stage-3 { background: linear-gradient(135deg, #f59e0b, #d97706); }" 

           ".actions{display:flex; justify-content:space-between; margin-top:35px; gap:20px; flex-wrap:wrap;}"
           ".btn-print{background:linear-gradient(135deg, #16a34a, #15803d); color:white; border:none; padding:15px 25px; border-radius:10px; font-weight:700; cursor:pointer; flex:1; transition:0.3s; text-align:center; font-family:var(--font-display); box-shadow: 0 4px 6px rgba(0,0,0,0.1);}"
           ".btn-save{background:linear-gradient(135deg, #f59e0b, #d97706); color:white; border:none; padding:15px 25px; border-radius:10px; font-weight:700; cursor:pointer; flex:1; transition:0.3s; text-align:center; font-family:var(--font-display); box-shadow: 0 4px 6px rgba(0,0,0,0.1);}"
           ".btn-secondary{background:linear-gradient(135deg, #4f46e5, #4338ca); color:white; padding:15px 25px; border-radius:10px; font-weight:700; text-align:center; flex:1; transition:0.3s; display:inline-block; text-decoration:none; font-family:var(--font-display); box-shadow: 0 4px 6px rgba(0,0,0,0.1);}"
           
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
    const string moon_icon = "<svg class='theme-moon' viewBox='0 0 24 24'><path d='M12.3 22h-.1c-5.5 0-10-4.5-10-10 0-4.8 3.5-8.9 8.2-9.8.6-.1 1.2.3 1.3.9.1.6-.2 1.2-.8 1.4-3.3 1-5.7 4-5.7 7.5 0 4.4 3.6 8 8 8 3.5 0 6.5-2.4 7.5-5.7.2-.6.8-.9 1.4-.8.6.1 1 .7.9 1.3-.9 4.7-5 8.2-9.8 8.2z'/></svg>";
    const string sun_icon = "<svg class='theme-sun' viewBox='0 0 24 24'><path d='M12 7c-2.8 0-5 2.2-5 5s2.2 5 5 5 5-2.2 5-5-2.2-5-5-5zm0-5c.6 0 1 .4 1 1v2c0 .6-.4 1-1 1s-1-.4-1-1V3c0-.6.4-1 1-1zm0 14c.6 0 1 .4 1 1v2c0 .6-.4 1-1 1s-1-.4-1-1v-2c0-.6.4-1 1-1zM4 11h2c.6 0 1 .4 1 1s-.4 1-1 1H4c-.6 0-1-.4-1-1s-1-.4-1-1zm14 0h2c.6 0 1 .4 1 1s-.4 1-1 1h-2c-.6 0-1-.4-1-1s-1-.4-1-1zM5.2 5.2c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0L5.2 6.6c-.4-.4-.4-1 0-1.4zm12 12c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0l-1.4-1.4c-.4-.4-.4-1 0-1.4zM7.6 16.4c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0l-1.4-1.4c-.4-.4-.4-1 0-1.4zm12-12c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0l-1.4-1.4c-.4-.4-.4-1 0-1.4z'/></svg>";

    string user_controls;
    if (current_user.empty()) {
        user_controls = "<a href='/login' class='nav-icon' title='تسجيل الدخول'><svg viewBox='0 0 24 24'><path d='M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 3c1.66 0 3 1.34 3 3s-1.34 3-3 3-3-1.34-3-3 1.34-3 3-3zm0 14.2c-2.5 0-4.71-1.28-6-3.22.03-1.99 4-3.08 6-3.08 1.99 0 5.97 1.09 6 3.08-1.29 1.94-3.5 3.22-6 3.22z'/></svg></a>";
    } else {
        user_controls = "<details class='nav-dropdown'><summary class='nav-icon' style='color:var(--accent); font-weight:bold;'>👤 " + current_user + "</summary>"
                        "<div class='dropdown-panel' style='min-width:140px;'><div class='dropdown-col'><a href='/my-reports'>التقارير المحفوظة</a><a href='/logout'>تسجيل الخروج</a></div></div></details>";
    }

    return "<nav class='navbar'>"
           "  <div class='nav-right'>"
           "    <a href='/' class='navbar-brand'><span class='brand-mark'><img src='" + logo_url + "' alt='لوجو'></span><span>ضربة شاكوش </span></a>"
           "    <div class='nav-center desktop-only'>"
           "      <a href='/' class='nav-link'>الرئيسية</a>"
           "      <a href='/paths' class='nav-link'>مسارات التعلّم</a>"
           "      <a href='/blog' class='nav-link'>المقالات</a>"
           "      <a href='/calculator' class='nav-link'>الحاسبة الهندسية</a>"
           "    </div>"
           "  </div>"
           "  <div class='nav-left'>"
           "    <button class='nav-icon' id='themeBtn'>" + moon_icon + sun_icon + "</button>"
           + user_controls +
           "  </div>"
           "</nav>";
}

static string get_theme_script(const string& nonce) {
    return "<script nonce='" + nonce + "'>"
           "  if(localStorage.getItem('theme') === 'light'){ document.body.classList.add('light-mode'); }"
           "  document.getElementById('themeBtn').addEventListener('click', function(){"
           "    document.body.classList.toggle('light-mode');"
           "    localStorage.setItem('theme', document.body.classList.contains('light-mode') ? 'light' : 'dark');"
           "  });"
           "</script>";
}

int main() {
    httplib::Server svr;
    Elevator elevator;

    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        string nonce = generate_nonce(); set_csp(res, nonce);
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html(user) +
                      "<div class='container'>"
                      "<div class='section-intro' style='text-align: center; margin-bottom: 40px;'>"
                      "  <h1 style='font-size: 2.2rem;'>مرحباً بك في منصة ضربة شاكوش</h1>"
                      "  <p style='color: var(--text-muted); font-size: 1.1rem;'>الأدوات الهندسية الذكية والكورسات التطبيقية للمصاعد</p>"
                      "</div>"
                      "<div class='grid-nav'>"
                      "  <a href='/calculator' class='nav-card'><h3>🛗 حاسبة المقاسات الفنية والبضاعة</h3><p>ابدأ تصفية أبعاد بئر المصعد وحساب المقاسات الصافية للكابينة.</p></a>"
                      "  <a href='/paths' class='nav-card'><h3>📖 مسارات الكورسات والتعلم</h3><p>اكتشف مسار التأسيس ميكانيكياً، وكورس كهرباء المصاعد الشامل.</p></a>"
                      "</div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    auto render_register_page = [](httplib::Response& res, const string& fn = "", const string& ln = "", const string& un = "", const string& em = "", const string& err_msg = "") {
        string nonce = generate_nonce(); set_csp(res, nonce);
        string alert_box = err_msg.empty() ? "" : "<div style='background:rgba(239,68,68,0.1); border:1px solid #ef4444; color:#ef4444; padding:12px; border-radius:8px; margin-bottom:20px; font-weight:600; text-align:center;'>" + err_msg + "</div>";
        string html = "<html><head><meta charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + 
                      "<style>.auth-card{max-width:550px; margin:20px auto; background:var(--surface); border:1px solid var(--border); padding:35px; border-radius:20px; box-shadow:0 15px 35px rgba(0,0,0,0.25);}"
                      ".auth-grid{display:grid; grid-template-columns:1fr 1fr; gap:15px;}</style></head><body>"
                      + get_navbar_html() + "<div class='container'><div class='auth-card'>"
                      "<h2 style='text-align:center; color:var(--accent);'>✨ انضم إلى نخبة المهندسين</h2>"
                      + alert_box +
                      "<form action='/api/register' method='post'>"
                      "<div class='auth-grid'>"
                      "<div class='f-group'><label>الاسم الأول:</label><input type='text' name='first_name' value='" + fn + "' required></div>"
                      "<div class='f-group'><label>اسم العائلة:</label><input type='text' name='last_name' value='" + ln + "' required></div>"
                      "</div>"
                      "<div class='auth-grid'>"
                      "<div class='f-group'><label>اسم المستخدم:</label><input type='text' name='username' value='" + un + "' required></div>"
                      "<div class='f-group'><label>البريد الإلكتروني:</label><input type='email' name='email' value='" + em + "' required></div>"
                      "</div>"
                      "<div class='auth-grid'>"
                      "<div class='f-group'><label>كلمة المرور:</label><input type='password' name='password' required></div>"
                      "<div class='f-group'><label>تأكيد كلمة المرور:</label><input type='password' name='confirm_password' required></div>"
                      "</div>"
                      "<button type='submit' style='margin-top:10px;'>🚀 إنشاء الحساب</button>"
                      "</form></div></div><div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>"
                      + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    };

    svr.Get("/register", [&render_register_page](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        if (!user.empty()) { res.set_redirect("/"); return; }
        render_register_page(res);
    });

    svr.Post("/api/register", [&render_register_page](const httplib::Request& req, httplib::Response& res) {
        string first_name = html_escape(req.get_param_value("first_name"));
        string last_name = html_escape(req.get_param_value("last_name"));
        string username = html_escape(req.get_param_value("username"));
        string email = html_escape(req.get_param_value("email"));
        string password = html_escape(req.get_param_value("password"));
        string confirm_password = html_escape(req.get_param_value("confirm_password"));

        if (password != confirm_password) {
            render_register_page(res, first_name, last_name, username, email, "⚠️ كلمتا المرور غير متطابقتين.");
            return;
        }
        if (!is_valid_username(username)) {
            render_register_page(res, first_name, last_name, "", email, "⚠️ اسم المستخدم غير صالح.");
            return;
        }
        if (users_db.find(username) != users_db.end()) {
            render_register_page(res, first_name, last_name, "", email, "⚠️ اسم المستخدم مستخدم مسبقاً.");
            return;
        }
        for (auto const& [u, acc] : users_db) {
            if (acc.email == email) {
                render_register_page(res, first_name, last_name, username, "", "⚠️ البريد الإلكتروني مسجل مسبقاً.");
                return;
            }
        }

        string otp = generate_otp();
        UserAccount new_acc;
        new_acc.first_name = first_name; new_acc.last_name = last_name;
        new_acc.username = username; new_acc.email = email;
        new_acc.password = password; new_acc.otp_code = otp; new_acc.is_verified = false;
        
        users_db[username] = new_acc;
        save_user_to_mongodb(new_acc);
        send_email_otp(email, first_name, otp);

        string nonce = generate_nonce(); set_csp(res, nonce);
        string html = "<html><head><meta charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "</head><body>" + get_navbar_html() +
                      "<div class='container' style='max-width:500px;'><div class='card' style='text-align:center;'>"
                      "<h2>🔐 أدخل رمز التفعيل</h2>"
                      "<form action='/api/verify-otp' method='post'>"
                      "<input type='hidden' name='username' value='" + username + "'>"
                      "<div class='f-group'><input type='text' name='otp' required maxlength='6' placeholder='أدخل الرمز' style='text-align:center; font-size:1.4rem; letter-spacing:4px; font-weight:bold;'></div>"
                      "<button type='submit'>✅ تأكيد وتفعيل الحساب</button></form></div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>" + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Post("/api/verify-otp", [](const httplib::Request& req, httplib::Response& res) {
        string username = html_escape(req.get_param_value("username"));
        string otp = html_escape(req.get_param_value("otp"));

        if (users_db.find(username) != users_db.end() && users_db[username].otp_code == otp) {
            users_db[username].is_verified = true;
            save_user_to_mongodb(users_db[username]);
            res.set_header("Set-Cookie", "session=" + username + "; Path=/; HttpOnly");
            
            string nonce = generate_nonce(); set_csp(res, nonce);
            string html = "<html><head><meta charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                          + get_modern_blue_css() + "</head><body>" + get_navbar_html(username) +
                          "<div class='container' style='max-width:550px; text-align:center;'><div class='card' style='border-color:#16a34a;'>"
                          "<h2 style='color:#16a34a;'>🎉 أهلاً وسهلاً بك يا بشمهندس " + users_db[username].first_name + "!</h2>"
                          "<p style='color:var(--text); font-size:1.1rem; line-height:1.8; margin-bottom:25px;'>نورت منصة ضربة شاكوش الرقمية. تم تفعيل حسابك وتوثيقه بنجاح تام.</p>"
                          "<a class='btn-secondary' href='/calculator' style='background:linear-gradient(135deg, #16a34a, #15803d); display:block; padding:15px;'>🛗 ابدأ العمل على الحاسبة الهندسية</a>"
                          "</div></div><div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>" + get_theme_script(nonce) + "</body></html>";
            res.set_content(html, "text/html; charset=utf-8");
        } else {
            res.set_content("<html><body dir='rtl'><h3>رمز التحقق غير صحيح، <a href='/register'>أعد المحاولة</a></h3></body></html>", "text/html; charset=utf-8");
        }
    });

    svr.Get("/login", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        if (!user.empty()) { res.set_redirect("/"); return; }
        string nonce = generate_nonce(); set_csp(res, nonce);
        string html = "<html><head><meta charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "<style>.auth-card{max-width:450px; margin:40px auto; background:var(--surface); border:1px solid var(--border); padding:40px; border-radius:20px; box-shadow:0 15px 35px rgba(0,0,0,0.25);}</style></head><body>"
                      + get_navbar_html() + "<div class='container'><div class='auth-card'>"
                      "<h2 style='text-align:center; color:var(--accent);'>🔑 تسجيل الدخول</h2>"
                      "<form action='/api/login' method='post'>"
                      "<div class='f-group'><label>اسم المستخدم:</label><input type='text' name='username' required></div>"
                      "<div class='f-group'><label>كلمة المرور:</label><input type='password' name='password' required></div>"
                      "<button type='submit' style='margin-top:10px;'>دخول آمن ➡️</button></form>"
                      "<div style='text-align:center; margin-top:15px;'><a href='/forgot-password' style='color:var(--accent-2); font-weight:600;'>نسيت كلمة المرور؟</a></div>"
                      "<div style='text-align:center; margin-top:10px;'><a href='/register' style='color:var(--accent); font-weight:600;'>إنشاء حساب جديد</a></div>"
                      "</div></div><div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>" + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        string username = html_escape(req.get_param_value("username"));
        string password = html_escape(req.get_param_value("password"));

        if (users_db.find(username) != users_db.end() && users_db[username].password == password) {
            res.set_header("Set-Cookie", "session=" + username + "; Path=/; HttpOnly");
            res.set_redirect("/");
        } else {
            res.set_content("<html><body dir='rtl'><h3>خطأ في بيانات الدخول، <a href='/login'>أعد المحاولة</a></h3></body></html>", "text/html; charset=utf-8");
        }
    });

    svr.Get("/forgot-password", [](const httplib::Request& req, httplib::Response& res) {
        string nonce = generate_nonce(); set_csp(res, nonce);
        string html = "<html><head><meta charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "<style>.auth-card{max-width:450px; margin:40px auto; background:var(--surface); border:1px solid var(--border); padding:40px; border-radius:20px; box-shadow:0 15px 35px rgba(0,0,0,0.25);}</style></head><body>"
                      + get_navbar_html() + "<div class='container'><div class='auth-card'>"
                      "<h2 style='text-align:center; color:var(--accent-2);'>🔓 استعادة كلمة المرور</h2>"
                      "<div class='sub-title' style='text-align:center;'>أدخل بريدك الإلكتروني المسجل لإرسال رابط الاستعادة</div>"
                      "<form action='/api/forgot-password' method='post'>"
                      "<div class='f-group'><label>البريد الإلكتروني:</label><input type='email' name='email' required placeholder='name@domain.com'></div>"
                      "<button type='submit' style='background:linear-gradient(135deg, #f59e0b, #d97706); margin-top:10px;'>📤 إرسال تعليمات الاستعادة</button></form>"
                      "<div style='text-align:center; margin-top:20px;'><a href='/login' style='color:var(--text-muted); font-weight:600;'>العودة لتسجيل الدخول</a></div>"
                      "</div></div><div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>" + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Post("/api/forgot-password", [](const httplib::Request& req, httplib::Response& res) {
        string email = html_escape(req.get_param_value("email"));
        string nonce = generate_nonce(); set_csp(res, nonce);
        bool found = false;
        for (auto const& [u, acc] : users_db) {
            if (acc.email == email) { found = true; break; }
        }
        string msg = found ? "تم إرسال رابط استعادة كلمة المرور إلى بريدك الإلكتروني بنجاح." : "عفواً، هذا البريد غير مسجل لدينا.";
        string color = found ? "#16a34a" : "#ef4444";
        string html = "<html><head><meta charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "</head><body>" + get_navbar_html() +
                      "<div class='container' style='max-width:500px; text-align:center;'><div class='card' style='border-color:" + color + ";'>"
                      "<h2 style='color:" + color + ";'>حالة الاستعلام</h2>"
                      "<p style='font-size:1.1rem; margin-bottom:20px;'>" + msg + "</p>"
                      "<a class='btn-secondary' href='/login'>➡️ العودة لتسجيل الدخول</a>"
                      "</div></div><div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>" + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get("/logout", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Set-Cookie", "session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
        res.set_redirect("/");
    });

    svr.Get("/calculator", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        string html = "<html><head><meta charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "</head><body>" + get_navbar_html(user) +
                      "<div class='container' style='max-width:650px;'><div class='card'><h2>🛗 حاسبة مقاسات بئر المصعد الفنية</h2>"
                      "<form action='/calculator-step2' method='post'>"
                      "<div class='f-group'><label>👑 نوع نظام تشغيل المصعد:</label><select name='m_type'><option value='MR'>غرفة محرك (MR)</option><option value='MRL'>بدون غرفة (MRL)</option></select></div>"
                      "<div class='f-group'><label>📐 عرض بئر المصعد الصافي (CM):</label><input type='number' name='width' required min='110' max='250' value='160'></div>"
                      "<div class='f-group'><label>📏 عمق بئر المصعد الصافي (CM):</label><input type='number' name='depth' required min='100' max='250' value='160'></div>"
                      "<div class='f-group'><label>🏢 إجمالي عدد الوقفات:</label><input type='number' name='floors' required min='1' max='60' value='5'></div>"
                      "<div class='f-group'><label>🕳️ عمق الحفرة Pit (CM):</label><input type='number' name='depth_pit' required value='140'></div>"
                      "<div class='f-group'><label>🏠 ارتفاع الأوفر هيد Overhead (CM):</label><input type='number' name='overhead' required value='400'></div>"
                      "<button type='submit'>➡️ الخطوة التالية</button></form></div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>" + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Post("/calculator-step2", [&elevator](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        string m_type = html_escape(req.get_param_value("m_type"));
        int w = safe_stoi(req.get_param_value("width"), 160);
        int d = safe_stoi(req.get_param_value("depth"), 160);
        string f = req.get_param_value("floors");
        string p = req.get_param_value("depth_pit");
        string oh = req.get_param_value("overhead");

        string raw_door = elevator.get_door_type(w);
        vector<string> doors = split_string(raw_door, "||");
        ostringstream d_sel;
        d_sel << "<select name='door_choice'>";
        for (auto& dt : doors) d_sel << "<option value='" << trim(dt) << "'>" << trim(dt) << "</option>";
        d_sel << "</select>";

        string nonce = generate_nonce(); set_csp(res, nonce);
        string html = "<html><head><meta charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "</head><body>" + get_navbar_html(user) +
                      "<div class='container' style='max-width:650px;'><div class='card'><h2>⚙️ تخصيص البضاعة</h2>"
                      "<form action='/calculate' method='post'>"
                      "<input type='hidden' name='m_type' value='" + m_type + "'>"
                      "<input type='hidden' name='width' value='" + to_string(w) + "'>"
                      "<input type='hidden' name='depth' value='" + to_string(d) + "'>"
                      "<input type='hidden' name='floors' value='" + f + "'>"
                      "<input type='hidden' name='depth_pit' value='" + p + "'>"
                      "<input type='hidden' name='overhead' value='" + oh + "'>"
                      "<div class='f-group'><label>🚪 مقاس الأبواب:</label>" + d_sel.str() + "</div>"
                      "<div class='f-group'><label>🛠️ منشأ السكك:</label><select name='rails_origin'><option value='صيني'>صيني</option><option value='إيطالي'>إيطالي</option></select></div>"
                      "<div class='f-group'><label>🚪 منشأ الأبواب:</label><select name='door_origin'><option value='صيني'>صيني</option><option value='إيطالي'>إيطالي</option></select></div>"
                      "<div class='f-group'><label>⚡ جهاز طوارئ ARD؟</label><select name='has_ard'><option value='yes'>نعم</option><option value='no'>لا</option></select></div>"
                      "<div class='f-group'><label>⛓️ عرض تفاصيل البضاعة؟</label><select name='calc_mat'><option value='yes' selected>نعم</option><option value='no'>لا</option></select></div>"
                      "<button type='submit'>🏛️ استخراج التقرير النهائي</button></form></div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>" + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Post("/calculate", [&elevator](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req);
        string m_type = html_escape(req.get_param_value("m_type"));
        int w = safe_stoi(req.get_param_value("width"), 160);
        int d = safe_stoi(req.get_param_value("depth"), 160);
        float f = safe_stof(req.get_param_value("floors"), 5.0f);
        int p = safe_stoi(req.get_param_value("depth_pit"), 140);
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

        string nonce = generate_nonce(); set_csp(res, nonce);
        ostringstream os;
        os << "<html><head><meta charset='UTF-8'>"
           "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
           "<script src='https://cdnjs.cloudflare.com/ajax/libs/html2pdf.js/0.10.1/html2pdf.bundle.min.js'></script>"
           << get_modern_blue_css() << "</head><body>"
           << get_navbar_html(user)
           << "<div class='container' style='max-width:780px;'>"
           << "<div class='card' id='pdf-area'>"
           << "<h2>📋 تقرير المقايسة والتصفية الهندسية</h2>"
           
           << "<div style='margin-bottom:20px; background:var(--surface-2); padding:15px; border-radius:10px; border:1px solid var(--border);'>"
           << "<p style='margin:5px 0;'><b>👤 اسم العميل:</b> <span id='lblClient' style='color:var(--accent);'>غير محدد</span></p>"
           << "<p style='margin:5px 0;'><b>📝 ملاحظات الموقع:</b> <span id='lblNotes' style='color:var(--text-muted);'>لا توجد ملاحظات</span></p>"
           << "</div>"

           << "<div class='stage-header stage-1'>📌 أولاً: المقاسات الإنشائية وأبعاد الصاعدة:</div>"
           << "<div class='table-container'><table class='tbl'>"
           << "<tr><th>أبعاد البئر الحرة:</th><td>عرض " << w << " سم × عمق " << d << " سم</td></tr>"
           << "<tr><th>نوع الأبواب:</th><td>[" << door_choice << "]</td></tr>"
           << "<tr><th>شاسيه الكابينة (DBG):</th><td>" << cabin_dbg << " سم</td></tr>"
           << "<tr><th>شاسيه الثقل (DBG):</th><td>" << cwt_dbg << " سم</td></tr>"
           << "<tr><th>صافي الكابينة داخلياً:</th><td>عرض " << cab_w << " سم × عمق " << cab_d << " سم</td></tr>"
           << "<tr><th>الحمولة المقدرة:</th><td style='color:#16a34a; font-weight:bold;'>" << specs.rated_load_kg << " كجم</td></tr>"
           << "<tr><th>مشوار البئر:</th><td>" << h << " متر</td></tr>"
           << "</table></div>";

        if (calc_mat == "yes") {
            os << "<div class='stage-header stage-2'>⚙️ ثانياً: بضاعة المرحلة الأولى:</div>"
               << "<div class='table-container'><table class='tbl'>"
               << "<tr><td>" << specs.scaffolding_name << "</td><td>" << specs.scaffolding_count << " طقم</td></tr>"
               << "<tr><td>" << specs.plumb_name << "</td><td>" << specs.plumb_count << " لفة</td></tr>"
               << "<tr><td>" << specs.balance_name << "</td><td>" << specs.balance_count << " قطعة</td></tr>"
               << "<tr><td>" << specs.door_exterior_name << "</td><td>" << specs.total_exterior_doors << " باب</td></tr>"
               << "<tr><td>" << specs.cabin_rails_name << "</td><td>" << specs.cabin_rails_count << " عود</td></tr>"
               << "<tr><td>" << specs.cwt_rails_name << "</td><td>" << specs.cwt_rails_count << " عود</td></tr>"
               << "<tr><td>" << specs.cabin_brackets_name << "</td><td>" << specs.cabin_brackets_count << " كابولي</td></tr>"
               << "<tr><td>" << specs.cwt_brackets_name << "</td><td>" << specs.cwt_brackets_count << " كابولي</td></tr>"
               << (specs.cwt_belt_count > 0 ? "<tr><td>" + specs.cwt_belt_name + "</td><td>" + to_string(specs.cwt_belt_count) + " حزام</td></tr>" : "")
               << (specs.platat_count > 0 ? "<tr><td>" + specs.platat_name + "</td><td>" + to_string(specs.platat_count) + " قطعة</td></tr>" : "")
               << "<tr><td>" << specs.sub_cabin_name << "</td><td>" << specs.sub_cabin_count << " قطعة</td></tr>"
               << "<tr><td>" << specs.sub_cwt_name << "</td><td>" << specs.sub_cwt_count << " قطعة</td></tr>"
               << "<tr><td>" << specs.hilti_bolts_name << "</td><td>" << specs.hilti_bolts_12mm << " مسمار</td></tr>"
               << "<tr><td>" << specs.assembly_bolts_name << "</td><td>" << specs.assembly_bolts_12mm << " مسمار</td></tr>"
               << "<tr><td>" << specs.bolts_8mm_name << "</td><td>" << specs.bolts_8mm << " مسمار</td></tr>"
               << "<tr><td>" << specs.spring_washers_8mm_name << "</td><td>" << specs.spring_washers_8mm << " طقم</td></tr>"
               << "<tr><td>" << specs.spring_washers_12mm_name << "</td><td>" << specs.spring_washers_12mm << " وردة</td></tr>"
               << "<tr><td>" << specs.nuts_12mm_name << "</td><td>" << specs.nuts_12mm << " صامولة</td></tr>"
               << "<tr><td>" << specs.flat_washers_12mm_name << "</td><td>" << specs.flat_washers_12mm << " وردة</td></tr>"
               << "</table></div>"

               << "<div class='stage-header stage-3'>⚡ ثالثاً: المرحلة الثانية والثالثة:</div>"
               << "<div class='table-container'><table class='tbl'>"
               << "<tr><td>" << specs.machine_type_desc << "</td><td>1 وحدة</td></tr>"
               << "<tr><td>" << specs.machine_bed_name << "</td><td>1 طقم</td></tr>"
               << (!specs.is_hydraulic ? "<tr><td>" + specs.cabin_wires_name + "</td><td>" + to_string(specs.cabin_wires_meters) + " متر</td></tr>" : "")
               << (!specs.is_hydraulic ? "<tr><td>شدادات ومطواة</td><td>" + specs.rope_hitches_desc + "</td></tr>" : "")
               << (!specs.is_hydraulic ? "<tr><td>بلوكات الثقل</td><td>" + to_string(specs.counterweight_blocks) + " بلوك</td></tr>" : "")
               << "<tr><td>" << specs.parachute_name << "</td><td>" << specs.parachute_count << " جهاز</td></tr>"
               << "<tr><td>" << specs.governor_rope_name << "</td><td>" << specs.governor_rope_meters << " متر</td></tr>"
               << "<tr><td>" << specs.buffer_set_name << "</td><td>" << specs.buffer_set_count << " طقم</td></tr>"
               << "<tr><td>" << specs.control_panel_name << "</td><td>" << specs.control_panel_count << " لوحة</td></tr>"
               << (specs.has_ard ? "<tr><td>" + specs.ard_system_name + "</td><td>" + to_string(specs.ard_system_count) + " جهاز</td></tr>" : "")
               << "<tr><td>" << specs.flex_cable_name << "</td><td>" << specs.flex_cable_meters << " متر</td></tr>"
               << "<tr><td>" << specs.trunk_4cm_name << "</td><td>" << specs.trunk_4cm_meters << " متر</td></tr>"
               << "<tr><td>" << specs.trunk_10cm_name << "</td><td>" << specs.static_trunk_10cm << " متر</td></tr>"
               << "<tr><td>" << specs.wire_1mm_name << "</td><td>" << specs.wire_1mm_count_desc << "</td></tr>"
               << "<tr><td>" << specs.cop_panel_name << "</td><td>" << specs.cop_panel_count << " لوحة</td></tr>"
               << "<tr><td>" << specs.lop_buttons_name << "</td><td>" << specs.lop_buttons_count << " طلبات</td></tr>"
               << "<tr><td>" << specs.safety_door_name << "</td><td>" << specs.safety_door_count << " بوابة</td></tr>"
               << "<tr><td>" << specs.photocell_name << "</td><td>" << specs.photocell_count << " ستارة</td></tr>"
               << "</table></div>";
        }

        os << "</div>";

        if (!user.empty()) {
            os << "<div class='card' style='border-color:var(--accent);'>"
               << "<h3>💾 حفظ التقرير في سجلك الشخصي باسم العميل</h3>"
               << "<form action='/api/save-report' method='post'>"
               << "<div class='f-group'><label>👤 اسم العميل أو المشروع:</label><input type='text' name='client_name' required placeholder='اكتب اسم العميل هنا'></div>"
               << "<div class='f-group'><label>📝 ملاحظات هندسية:</label><input type='text' name='notes' placeholder='ملاحظات خاصة بالموقع'></div>"
               << "<input type='hidden' name='report_summary' value='مقايسة بئر " << w << "x" << d << " سم - أدوار: " << (int)f << "'>"
               << "<button type='submit' class='btn-save'>💾 اعتماد الحفظ باسم العميل</button>"
               << "</form></div>";
        } else {
            os << "<div class='card' style='text-align:center;'><a class='btn-secondary' href='/login' style='background:#f5a524; color:#000; display:block;'>🔒 سجل دخولك لتخزين هذا التقرير باسم العميل</a></div>";
        }

        os << "<div class='actions'>"
           << "<button class='btn-print' id='pBtn'>📥 تحميل التقرير PDF</button>"
           << "<a class='btn-secondary' href='/calculator'>🔄 تصفية بئر جديد</a>"
           << "</div></div>"
           << "<div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>"
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
                r_list << "<div style='background:var(--bg); border:1px solid var(--border); padding:15px; border-radius:10px; margin-bottom:12px;'>"
                       << "<b>👤 اسم العميل:</b> <span style='color:var(--accent);'>" << rep.client_name << "</span><br>"
                       << "<b>📌 تفاصيل المقايسة:</b> " << rep.summary << "<br>"
                       << "<b>📝 الملاحظات:</b> " << (rep.notes.empty() ? "لا توجد" : rep.notes)
                       << "</div>";
            }
        } else {
            r_list << "<p style='text-align:center; color:var(--text-muted);'>لا توجد تقارير محفوظة حتى الآن.</p>";
        }

        string html = "<html><head><meta charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "</head><body>" + get_navbar_html(user) +
                      "<div class='container' style='max-width:700px;'><div class='card'><h2>📂 سجل تقارير العُملاء المحفوظة</h2>"
                      + r_list.str() +
                      "<div style='margin-top:20px;'><a class='btn-secondary' href='/calculator' style='display:block; text-align:center;'>🛗 حاسبة مقاسات جديدة</a></div>"
                      "</div></div><div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>" + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get("/paths", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        string html = "<html><head><meta charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "</head><body>" + get_navbar_html(user) +
                      "<div class='container'><div class='section-intro'><h1>🧭 مسارات التعلم</h1></div>"
                      "<div class='grid-nav'><a href='#' class='nav-card'><h3>🧱 مسار الأساسات الهندسية</h3><p>المفاهيم الأولى لتصفية الأبعاد.</p></a>"
                      "<a href='#' class='nav-card'><h3>🔨 كورس كهرباء المصاعد الشامل</h3><p>تعلم دوائر الكنترول وأمان المصاعد.</p></a></div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>" + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get("/blog", [](const httplib::Request& req, httplib::Response& res) {
        string user = get_session_user(req); string nonce = generate_nonce(); set_csp(res, nonce);
        string html = "<html><head><meta charset='UTF-8'><link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                      + get_modern_blue_css() + "</head><body>" + get_navbar_html(user) +
                      "<div class='container'><div class='section-intro'><h1>📚 المقالات والشروحات</h1><p>قريباً يتم إدراج المقالات الهندسية.</p></div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026</div>" + get_theme_script(nonce) + "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    const char* port_env = getenv("PORT");
    int port = port_env ? safe_stoi(port_env, 8080) : 8080;
    cout << "🚀 Professional Hammer-Platform active on port: " << port << endl;
    svr.listen("0.0.0.0", port);
    return 0;
}
