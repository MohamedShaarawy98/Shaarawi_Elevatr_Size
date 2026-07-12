/*                     <  وَأَن لَّيسَ لِلإِنسَانِ إِلاَّ مَا سَعَى * وَأَنَّ سَعْيَهُ سَوْفَ يُرَى * ثُمَّ يُجْزَاهُ الْجَزَاء الأَوْفَى  >

                               ============================================================
                               =                                                          =
                               =                  منصة ضربة شاكوش الرقمية                 =
                               =                                                          =
                               ============================================================
 */          

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

using namespace std;

// ============================================================================
// متغيرات التحكم في جاهزية أنواع المصاعد (اجعل القيمة false لإغلاق النوع وإظهار رسالة التعديل)
// ============================================================================
const bool IS_MR_READY        = true;   // المصعد القياسي (غرفة)
const bool IS_MRL_READY       = false;  // المصعد الجيرليس (بدون غرفة)
const bool IS_HYDRAULIC_READY = false;  // المصعد الهيدروليك

// بنية بيانات لتحديد معدل الطلبات لحماية السيرفر من هجمات الحرمان من الخدمة (Rate Limiting)
struct RateLimitInfo {
    int count = 0;
    chrono::steady_clock::time_point reset_time;
};

// قاموس لتتبع معدل طلبات المستخدمين وحماية موارد الخادم
static map<string, RateLimitInfo> ip_tracker;
static mutex rate_limit_mtx; 
const int MAX_REQUESTS_PER_MINUTE = 20; 

static string CF_VERIFY_SECRET = getenv("CF_VERIFY_SECRET") ? getenv("CF_VERIFY_SECRET") : "";
static string SECURE_HEADER_NAME = "X-Verify-Secret"; 

// دوال مساعدة آمنة لتحويل النصوص وتطهير البيانات لمنع ثغرات الالتفاف الرقمي والـ Crash
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

static string generate_nonce() {
    random_device rd; mt19937_64 gen(rd());
    uint64_t a = gen(), b = gen();
    ostringstream oss; oss << hex << a << b;
    return oss.str();
}

static void set_security_headers(httplib::Response& res) {
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-XSS-Protection", "1; mode=block");
    res.set_header("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    res.set_header("Referrer-Policy", "strict-origin-when-cross-origin");
    res.set_header("Server", "Hammer-Engine/1.1");
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

// ============================================================================
// الكلاس الهندسي للمصعد - تم الفحص والتدقيق الكامل لجميع المتغيرات والدوال
// ============================================================================
class Elevator {
private:
    // 🔒 [قسم خاص - Private]: أسماء ومسميات البضاعة الثابتة
    const string name_door_auto     = "باب اتوماتيك تخزين / سنتر";
    const string name_door_semi     = "باب نصف اتوماتيك";
    const string name_rail_16       = "سكة كابينة 16 مللي";
    const string name_rail_9        = "سكة كابينة 9 مللي";
    const string name_rail_5        = "سكة تقل 5 مللي";
    const string name_bracket_cab   = "كوابيل كابينة حسب مقاس السكة";
    const string name_bracket_cwt   = "كوابيل التقل القياسية";
    const string name_scaffolding   = "سقالة  ";
    const string name_plumb_lines   = "شواكيل خيط عيار هندسي";
    const string name_balance_tube  = "تيوب ميزان المياه للميزانية";
    const string name_hilti_bolt    = "مسمار هلتي 12 مللي جداري";
    const string name_assem_bolt    = "مسمار تجميع 12 مللي للسمك";
    const string name_bolt_8mm      = "مسمار 8 مللي لتجميع الثقل";
    const string name_washer_spring = "وردة سوسته 12 مللي ";
    const string name_nut_12mm      = "صامولة 12 مللي ";
    const string name_washer_flat   = "وردة صاج 12 مللي ";
    const string name_spring_8mm    = "وردة سوستة وصامولة 8 مللي للثقل";
    const string name_sub_cab       = "سبورتينات كابينة ";
    const string name_sub_cwt       = "سبورتينات ثقل ";
    const string name_door_casing   = "تلبيس حلوق الأبواب الخارجية";
    const string name_ceiling_cut   = "تفتيح وتجهيز فتحات سقف البئر";
    const string name_rubber_pads   = "طقم ربر كراسي الماكينة للغرفة";
    const string name_wire_6_5mm    = "ويرات فولاذية 6.50 مللي";
    const string name_wire_11mm     = "ويرات فولاذية 11 مللي ";
    const string name_rope_hitch    = "شداد حبل ";
    const string name_rope_clamp    = "زرجينة حبل ";
    const string name_parachute     = "جهاز براشوت الأمان ";
    const string name_governor_rope = "حبل براشوت منظم السرعة";
    const string name_buffer_set    = "طقم بفر الهيدروليك";
    const string name_counterweight = "بلوكات زهر حديد للثقل";
    const string name_shoes_cab     = "كراسي  الكابينة";
    const string name_shoes_cwt     = "كراسي وتزليق الثقل";
    const string name_control_panel = "لوحة تحكم (كنترول) ";
    const string name_ard_system    = "كنترول طوارئ إنقاذ آلي (ARD)";
    const string name_ctrl_fischer  = "مسامير وفيشر 12 مللي لتثبيت الكنترول";
    const string name_inspect_box   = "علبة صيانة فوق ظهر الكابينة";
    const string name_charger_batt  = "شاحن وبطارية طوارئ الكنترول";
    const string name_emerg_alarm   = "جرس وبطارية طوارئ ";
    const string name_flex_cable    = "كيبل مرن  (Traveling Cable)";
    const string name_flex_holder   = "حامل  الكيبل المرن";
    const string name_trunk_4cm     = "ترنكات بلاستيك 4 سم ";
    const string name_trunk_10cm    = "ترنكات بلاستيك 10 سم ";
    const string name_trunk_screws  = "مسامير وفيشر 8 مللي لتثبيت الترنكات";
    const string name_oiler_set     = "طقم مزايت";
    const string name_wire_6mm      = "سلك كهرباء 6 مللي رئيسي";
    const string name_wire_11mm_c   = "سلك 10 مللي في حالة الجيربوكس";
    const string name_wire_1rem     = "سلك طعام";
    const string name_wire_1mm      = "سلك إشارة وتوصيل 1 مللي لفة";
    const string name_net_cable     = "سلك نت  شاشات وانتركم";
    const string name_cop_panel     = "لوحة طلبات داخلية الكابينة (COP)";
    const string name_lop_buttons   = "طلبات خارجية للأدوار (LOP)";
    const string name_intercom      = "جهاز انتركم  ";
    const string name_safety_door   = "باب داخلي سلامة لحماية الركاب";
    const string name_photocell     = "جهاز فوتوسيل (ستارة سحرية) للأمان";
    const string name_stop_magnet   = "مغناطيس توقف نهائي لفل";
    const string name_count_magnet  = "مغناطيس عداد  الادوار";
    const string name_limit_sw      = "ليميت سويتش ميكانيكي ";
    const string name_limit_sensors = "حساسات مغناطيسية عيارية";
    const string name_floor_poles   = "بولات المغناطيس  ";

public:
    // دوال الأبعاد واللوجيك الهندسي
    string get_door_type(int sa) {
        if (sa >= 210 && sa <= 250)      return "Auto 80 CO || Auto 90 CO || Auto 100 CO";
        else if (sa >= 190 && sa < 210)  return "Auto 80 CO || Auto 90 CO || Auto 100 SI";
        else if (sa >= 175 && sa < 190)  return "Auto 80 CO || Auto 100 SI || Auto 90 SI";
        else if (sa >= 167 && sa < 175)  return "Auto 90 SI || Auto 80 CO";
        else if (sa >= 160 && sa < 167)  return "Auto 90 SI || Auto 70 CO";
        else if (sa >= 155 && sa < 160)  return "Auto 80 SI || Auto 70 CO";
        else if (sa >= 145 && sa < 155)  return "Auto 80 SI || S";
        else if (sa >= 128 && sa < 145)  return "Auto 70 SI";
        else if (sa >= 120 && sa < 128)  return "Semi Auto 80";
        else if (sa >= 110 && sa < 120)  return "Semi Auto 70";
        return "تصفية خاصة - مراجعة يدوية";
    }
    int get_cabin_width(int cw) { return cw - 40; }
    int get_cabin_depth(int cd) { return cd - 60; }
    
    int get_cabin_dbg(int w) { return w - 30; }
    
    int get_cwt_dbg(int w) {
        if (w >= 100 && w <= 110) return 72;
        if (w > 110 && w <= 120) return 82;
        if (w > 120 && w <= 125) return 92;
        if (w > 125 && w <= 210) return 102;
        return 0;
    }
    
    float get_shaft_height(float f, float pit_m, float overhead_m, string t) {
        float h = (f - 1) * 3.2f + pit_m + overhead_m;
        if (t == "MRL") return h + 1.5f;
        else if (t == "Hydraulic") return h - 0.5f;
        return h;
    }

    string get_welding_rods() { return "باكيت سلك لحام"; }
    string get_grinding_discs() { return "باكيت اسطوانات صاروخ قطعية"; }
    string get_cabin_type(int width, int depth) { (void)width; (void)depth; return "كابينة"; }
    string get_cwt_position(int width, int depth) { (void)width; (void)depth; return "تقل"; }

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

    // هيكلية تقرير المقايسة الشاملة المصلحة بالكامل
    struct FullSpecificationReport {
        // المرحلة الأولى
        string door_exterior_name;
        int total_exterior_doors;
        string cabin_rails_name;
        string cabin_rails_type; 
        int cabin_rails_count;
        string cwt_rails_name;
        string cwt_rails_type;  
        int cwt_rails_count;
        string cabin_brackets_name;
        int cabin_brackets_count;
        string cwt_brackets_name;
        int cwt_brackets_count;
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
        string door_casing_note;
        string scaffolding_name;
        int scaffolding_count = 1;
        string plumb_name;
        int plumb_count = 1;
        string balance_name;
        int balance_count = 1;

        // المرحلة الثانية
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
        string cabin_design_type;
        string cwt_design_type;
        float cabin_wires_meters;
        string cabin_wires_name;
        int rope_hitches_count;
        int rope_clamps_count;
        int counterweight_blocks;

        // المرحلة الثالثة
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
    };

    FullSpecificationReport compile_full_specification(int w, int d, int floors, const string& type) {
        FullSpecificationReport r;
        
        int cab_w = get_cabin_width(w);
        int cab_d = get_cabin_depth(d);
        int cwt_dbg = get_cwt_dbg(w); 
        
        r.rated_load_kg = calculate_rated_load(cab_w, cab_d);
        r.ceiling_cut_name    = name_ceiling_cut;
        r.parachute_name      = name_parachute;
        r.governor_rope_name  = name_governor_rope;
        r.buffer_set_name     = name_buffer_set;

        r.scaffolding_name = name_scaffolding;
        r.plumb_name       = name_plumb_lines;
        r.balance_name     = name_balance_tube;
        
        r.total_exterior_doors = floors * 1;
        if (w >= 128) {
            r.door_exterior_name = name_door_auto ;
            r.door_casing_note   = name_door_casing + " (يلزم تركيب علب التلبيس لحماية  الأبواب الأوتوماتيك)";
        } else {
            r.door_exterior_name = name_door_semi ;
            r.door_casing_note   = "لا يحتاج تلبيس حلوق (الأبواب المتاحة نصف أوتوماتيكية)";
        }

        float travel_path = ((floors - 1) * 3.2f) + 5.0f;
        int single_side_rails = static_cast<int>(travel_path / 5.0f) + 1;
        
        if (cab_d >= 150) {
            r.cabin_rails_type = name_rail_16;
        } else {
            r.cabin_rails_type = name_rail_9;
        }
        r.cabin_rails_count = single_side_rails * 2;
        r.cabin_rails_name  = r.cabin_rails_type;

        r.cabin_brackets_count = floors * 8; 
        r.cabin_brackets_name  = name_bracket_cab;
        r.sub_cabin_count      = r.cabin_brackets_count; 
        r.sub_cabin_name       = name_sub_cab;

        if (type == "Hydraulic") {
            r.cwt_rails_name       = "لا يوجد (نظام هيدروليك)";
            r.cwt_rails_count      = 0;
            r.cwt_brackets_count   = 0;
            r.cwt_brackets_name    = "لا يوجد (نظام هيدروليك)";
            r.sub_cwt_count        = 0;
            r.sub_cwt_name         = "لا يوجد (نظام هيدروليك)";
        } else {
            r.cwt_rails_name       = name_rail_5;
            r.cwt_rails_count      = single_side_rails * 2;
            r.cwt_brackets_count   = floors * 8; 
            r.cwt_brackets_name    = name_bracket_cwt;
            r.sub_cwt_count        = r.cwt_brackets_count; 
            r.sub_cwt_name         = name_sub_cwt;
        }

        // إسناد مسميات المسامير
        r.hilti_bolts_name         = name_hilti_bolt;
        r.assembly_bolts_name      = name_assem_bolt;
        r.bolts_8mm_name           = name_bolt_8mm;
        r.spring_washers_8mm_name  = name_spring_8mm;
        r.spring_washers_12mm_name = name_washer_spring;
        r.nuts_12mm_name           = name_nut_12mm;
        r.flat_washers_12mm_name   = name_washer_flat;

        r.hilti_bolts_12mm    = (floors * 2) + (r.total_exterior_doors * 8);
        r.assembly_bolts_12mm = (r.cabin_brackets_count / 2) + 30 + ((r.cabin_rails_count - 2) * 8);
        r.bolts_8mm           = (type == "Hydraulic") ? 0 : ((r.cwt_rails_count - 2) * 8);
        r.spring_washers_8mm  = r.bolts_8mm;
        
        r.spring_washers_12mm = r.hilti_bolts_12mm + r.assembly_bolts_12mm;
        r.nuts_12mm           = r.assembly_bolts_12mm; 
        r.flat_washers_12mm   = (r.assembly_bolts_12mm * 2) + r.hilti_bolts_12mm + r.sub_cabin_count;

        if (type == "Hydraulic") {
            r.machine_type_desc   = "بستم ومجموعة ضخ هيدروليكية بالكامل (Hydraulic Pump Pack)";
            r.machine_rubber_note = "لا يوجد)";
            r.cabin_design_type   = get_cabin_type(w, d) + " هيدروليك جانبية";
            r.cwt_design_type     = "لا يوجد تقل في الهيدرولي";
            r.cabin_wires_name    = " جاري التعديل ";
            r.cabin_wires_meters  = 0;
            r.rope_hitches_count  = 0;
            r.rope_clamps_count   = 0;
            r.counterweight_blocks = 0;
        } 
        else if (type == "MRL") {
            r.machine_type_desc   = "ماكينة بدون غرف جيرليس متطورة (Gearless MRL)";
            r.machine_rubber_note = "لا يوجد";
            r.cabin_design_type   = get_cabin_type(w, d) + " نظام تعليق جيرليس عياري";
            r.cwt_design_type     = get_cwt_position(w, d) + " جانبي / خلفي موفر للمساحة";
            
            r.cabin_wires_name   = name_wire_6_5mm;
            r.cabin_wires_meters = floors * 4.5f * 2.0f;
            
            r.rope_hitches_count = (r.rated_load_kg >= 800) ? 6 : 4; 
            r.rope_clamps_count  = r.rope_hitches_count * 2;
            r.counterweight_blocks = (r.rated_load_kg / 50) + 10; 
        } 
        else { 
            r.machine_type_desc   = "ماكينة بصندوق تروس جيربوكس قياسية (Geared Traction Machine)";
            r.machine_rubber_note = name_rubber_pads + " (يلزم تركيب طقم الربر لامتصاص الاهتزازات)";
            r.cabin_design_type   = get_cabin_type(w, d) + " نظام تعليق عادي قياسي 1:1";
            r.cwt_design_type     = get_cwt_position(w, d) + " خلفي قياسي عمق البئر متناسق";
            
            r.cabin_wires_name   = name_wire_11mm;
            r.cabin_wires_meters = (floors * 5.0f) + 2.0f;
            
            r.rope_hitches_count = (r.rated_load_kg >= 800) ? 5 : 4;
            r.rope_clamps_count  = r.rope_hitches_count * 2;
            r.counterweight_blocks = (r.rated_load_kg / 40) + 8;
        }

        r.governor_rope_meters = (travel_path * 2.0f) + 4.0f; 
        
        if (type != "Hydraulic" && cwt_dbg > 6) {
            r.cwt_blocks_weight_desc = "مقاس شواكيل عرض قالب الزهر = " + to_string(cwt_dbg - 6) + " CM صفي";
        } else {
            r.cwt_blocks_weight_desc = "لا يوجد";
        }

        r.control_panel_name = name_control_panel;
        r.ard_system_name    = name_ard_system;
        r.ctrl_fischer_name  = name_ctrl_fischer;
        r.inspect_box_name   = name_inspect_box;
        r.charger_batt_name  = name_charger_batt;
        r.emerg_alarm_name   = name_emerg_alarm;
        r.flex_holder_name   = name_flex_holder;
        r.trunk_10cm_name    = name_trunk_10cm;
        r.oiler_set_name     = name_oiler_set;
        r.wire_6mm_name      = name_wire_6mm;
        r.cop_panel_name     = name_cop_panel;
        r.intercom_name      = name_intercom;
        r.stop_magnet_name   = name_stop_magnet;
        r.count_magnet_name  = name_count_magnet;
        r.limit_sw_name      = name_limit_sw;
        r.limit_sensors_name = name_limit_sensors;
        r.floor_poles_name   = name_floor_poles;
        
        r.flex_cable_name   = name_flex_cable;
        r.flex_cable_meters = (floors * 4.0f) + 7.0f;
        
        r.trunk_4cm_name     = name_trunk_4cm;
        r.trunk_4cm_meters   = travel_path; 
        r.trunk_screws_name  = name_trunk_screws;
        r.trunk_screws_8mm   = floors * 4;  
        
        r.wire_1mm_name = name_wire_1mm;
        int temp_coils = 0;
        if (floors >= 2 && floors <= 4)       temp_coils = 4;
        else if (floors >= 5 && floors <= 7)  temp_coils = 5;
        else if (floors >= 8 && floors <= 10) temp_coils = 8;
        else if (floors >= 11 && floors <= 15) temp_coils = 10;
        else if (floors >= 16 && floors <= 20) temp_coils = 16;
        else                                  temp_coils = static_cast<int>(floors * 0.8f);

        r.wire_1mm_coils = temp_coils;
        r.wire_1mm_count_desc = to_string(temp_coils) + " لفة"; 

        r.net_cable_name   = name_net_cable;
        r.net_cable_meters = travel_path + (floors * 2.0f) + 5.0f;
        
        r.lop_buttons_name  = name_lop_buttons;
        r.lop_buttons_count = r.total_exterior_doors; 

        if (w >= 128) {
            r.photocell_name = name_photocell;
            r.photocell_note = "نظام أوتوماتيكي بالكامل: يتضمن ستارة الأشعة التلقائية الفوتوسيل أوتوماتيك.";
            r.safety_door_name = "مشغل باب المقصورة الداخلي التلقائي التزامن للفتح والغلق الآمن";
        } else {
            r.photocell_name = "غير مطلوب للنظام النص أوتوماتيك";
            r.photocell_note = "المصعد نصف أوتوماتيكي: يتم الاستعاضة عنه بكامّة الكالون الميكانيكية بالبئر.";
            r.safety_door_name = name_safety_door + " (يلزم تركيب شاتر أو باب سلامة لحماية الراكب داخل المقصورة)";
        }

        r.floor_poles_count = floors * 6; 

        return r;
    }
};

// الهياكل الثابتة لبيانات المقالات والمسارات التعليمية بالمنصة
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
        { "darbat" , "🔨" , "كورس كهرباء المصاعد الشامل"  , "كورس تطبيقية متخصص في تعليم كل شيء يخص كهرباء الدوائر، الكنترولات، وتوصيل كوابل الأمان."}
    }; 
}

static vector<Lesson> get_lessons() {
    return {
        { "intro-shaft-dimensions", "basics", "video",
          "مقدمة هندسية: فهم أبعاد بئر المصعد الصافية",
          "شرح فني يوضح الفرق بين البئر الحر والـ Pit والـ Overhead وأهميتهم الكبرى في التصفية الميكانيكية.",
          "<p>بئر المصعد بيتكون من 3 قياسات أساسية لازم تكون دقيقة جداً قبل البدء في أي تصفية هندسية: "
          "العرض والعمق الحُرّين للبئر، عمق حفرة الـ Pit أسفل المحطة الأخيرة، وارتفاع الـ Overhead فوق آخر وقفة للمصعد.</p>"
          "<p>أي خطأ بسيط في أي قياس من القياسات الثلاثة بيأثر مباشرة على نوع الباب المتاح ومقاس الكابينة الصافي، "
          "وده اللي بتحسبه الحاسبة الذكية أوتوماتيك من غير الحاجه لجدول ورقي معقد.</p>",
          "https://www.youtube.com/embed/ZluG-pfc2HY", 1},

        { "door-types-explained", "doors", "video",
          "شرح أنواع أبواب المصاعد الأوتوماتيكية والنص أوتوماتيك Auto / Semi",
          "فيديو تفصيلي يوضح الفرق بين أبواب السنتر CO وأبواب التلسكوب الجانبية SI وإزاي تختار المقاس والنوع المناسب لبئرك وعرض فتحة الصاعدة.",
          "",
          "https://www.youtube.com/embed/ZluG-pfc2HY", 1},

          {"darbat-shakosh-one" ,"darbat" , "video", 
            "توصيل الكنترول وقفل دوائر الأمان - الدرس 2",
            "أهمية ترتيب خطوات تأسيس خطوط الكهرباء وربط دوائر السلسلة للامان وضمان الشغل النضيف والمطابق للمواصفات.",
          "<p> أولاً: يتم التأكد من كهرباء المبنى وتغذية المصدر سواء كانت الفازة 220 فولت أو 380 فولت لعمل وتأسيس لوحة الكنترول على هذا الأساس السليم.</p>"
          "<p> ثانياً: تركيب الكنترول وتوصيل المحرك (الماكينة) وقفل دوائر السيفتي الرئيسية {الشوكة - الكالون - الاستوب} ثم اختبار حركة المصعد السريعة والبطيئة لضمان الاستجابة.</p>",
          "https://www.youtube.com/embed/ZluG-pfc2HY", 2},

          {"darbat-shakosh-tow" ,"darbat" , "article", 
            "المبادئ الأولى لكهرباء وكروت المصاعد - الدرس 1",
            "نظرة عامة على لوحة التحكم والروابط الكهربائية وتغذية الفرامل ومغناطيس التهدئة والتوقف الفني.",
          "<p> أولاً: يتم التأكد من كهرباء المبنى وتغذية المصدر سواء كانت الفازة 220 فولت أو 380 فولت لعمل وتأسيس لوحة الكنترول على هذا الأساس السليم.</p>"
          "<img src='https://media.darbat-shakosh.com/channels4_profile%20(1).jpg' style='width:100%; max-width:600px; display:block; border-radius:8px; margin:20px auto; border:1px solid var(--border);' alt='خريطة ومخطط المصعد الفني'>"
          "<p> ثانياً: تركيب الكنترول وتوصيل المحرك (الماكينة) وقفل دوائر السيفتي الرئيسية {الشوكة - الكالون - الاستوب} ثم اختبار حركة المصعد السريعة والبطيئة لضمان الاستجابة.</p>",
          "https://www.youtube.com/embed/ZluG-pfc2HY", 1}
    };
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

           ".container{max-width:900px; margin:0 auto; padding:50px 20px; flex:1; width:100%;}"
           ".card{position:relative; background:var(--surface); border:1px solid var(--border); padding:40px; border-radius:12px; box-shadow:0 20px 25px -5px rgba(0,0,0,0.3); text-align:right; transition: background-color 0.3s;}"
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

           ".table-container{position:relative; width:100%; overflow-x:auto; background:var(--bg); border-radius:8px; border:1px solid var(--border); margin-top:20px;}"
           ".tbl{width:100%; border-collapse:collapse; text-align:right; margin-bottom: 20px;}"
           ".tbl th{background:var(--surface); padding:15px; color:var(--accent); font-weight:600; border-bottom:1px solid var(--border); font-size:1rem; text-align:right; width:45%;}"
           ".tbl td{padding:15px; border-bottom:1px solid var(--border); color:var(--text); font-size:1rem; font-weight:600; text-align:right; font-family:var(--font-mono);}"
           ".actions{display:flex; justify-content:space-between; margin-top:35px; gap:20px;}"
           ".btn-print{background:linear-gradient(135deg, #16a34a, #15803d); color:white; border:none; padding:15px 25px; border-radius:8px; font-weight:700; cursor:pointer; flex:1; transition:0.3s; text-align:center; font-family:var(--font-display); box-shadow: 0 4px 6px rgba(0,0,0,0.1);}"
           ".btn-print:hover{background:linear-gradient(135deg, #15803d, #166534);}"
           ".btn-secondary{background:linear-gradient(135deg, #4f46e5, #4338ca); color:white; padding:15px 25px; border-radius:8px; font-weight:700; text-align:center; flex:1; transition:0.3s; display:inline-block; text-decoration:none; font-family:var(--font-display); box-shadow: 0 4px 6px rgba(0,0,0,0.1);}"
           ".btn-secondary:hover{background:linear-gradient(135deg, #4338ca, #3730a3);}"
           ".grid-nav{display:grid; grid-template-columns:repeat(auto-fit, minmax(280px, 1fr)); gap:25px; width:100%;}"
           ".nav-card{position:relative; background:var(--surface); border:1px solid var(--border); padding:30px; border-radius:12px; text-decoration:none; color:var(--text); transition:0.3s; display:flex; flex-direction:column; text-align:right;}"
           ".nav-card:hover{border-color:var(--accent); transform:translateY(-3px); box-shadow:0 10px 20px rgba(0,0,0,0.2);}"
           ".nav-card h3{color:var(--accent); font-size:1.3rem; margin:0 0 12px 0;}"
           ".nav-card p{color:var(--text-muted); font-size:0.95rem; line-height:1.6; margin:0;}"

           ".section-intro{margin-bottom:30px; text-align:right;}"
           ".section-intro h1{color:var(--text); font-size:1.7rem; font-weight:800; margin:0 0 8px 0;}"
           ".section-intro p{color:var(--text-muted); font-size:1rem; line-height:1.7; margin:0;}"
           ".lesson-tag{display:inline-flex; align-items:center; gap:5px; font-size:0.78rem; font-weight:700; padding:4px 10px; border-radius:20px; margin-bottom:12px; width:fit-content;}"
           ".tag-article{background:rgba(56,189,248,0.14); color:var(--accent);}"
           ".tag-video{background:rgba(245,165,36,0.16); color:var(--accent-2);}"
           ".video-embed{position:relative; width:100%; aspect-ratio:16/9; border-radius:10px; overflow:hidden; background:#000; margin:20px 0; border:1px solid var(--border); box-shadow: 0 4px 12px rgba(0,0,0,0.3);}"
           ".video-embed iframe{position:absolute; inset:0; width:100%; height:100%; border:0;}"
           ".lesson-body{color:var(--text); font-size:1.02rem; line-height:1.9; background: var(--bg); padding: 20px; border-radius: 8px; border: 1px solid var(--border); margin-top: 15px;}"
           ".lesson-body p{margin:0 0 16px 0;}"
           ".track-list{display:flex; flex-direction:column; gap:14px; margin-top:10px;}"
           ".track-item{display:flex; align-items:flex-start; gap:16px; background:var(--bg); border:1px solid var(--border); border-radius:10px; padding:18px 20px; text-decoration:none; transition:0.2s;}"
           ".track-item:hover{border-color:var(--accent); transform:translateX(-3px);}"
           ".track-order{flex-shrink:0; width:34px; height:34px; border-radius:8px; background:var(--surface-2); color:var(--accent); font-family:var(--font-mono); font-weight:700; display:flex; align-items:center; justify-content:center; font-size:0.95rem;}"
           ".track-item-title{color:var(--text); font-weight:700; font-size:1.02rem; margin-bottom:4px;}"

           ".footer{margin-top:auto; padding:25px 0; font-size:15px; color:var(--text-muted); text-align:center; border-top:1px solid var(--border); background-color:var(--surface); font-weight:600;}"
           
           "@media print{"
           "  body, .container, #pdf-area { background: #121826 !important; color: #f3f4f6 !important; height: auto !important; overflow: visible !important; min-height: unset !important; padding: 0 !important; margin: 0 !important; width: 100% !important; }"
           "  .card { box-shadow: none !important; border: none !important; padding: 20px !important; background: #121826 !important; width: 100% !important; height: auto !important; overflow: visible !important; position: static !important; }"
           "  .table-container { overflow: visible !important; width: 100% !important; border: 1px solid #232c3f !important; background: #0a0e16 !important; }"
           "  .tbl { width: 100% !important; table-layout: fixed !important; }"
           "  .btn-print, .btn-secondary, h3, .navbar, .flags-strip, .footer { display: none !important; }"
           "  .card::before, .card::after { display: none !important; }"
           "}"
           "</style>";
}

static string get_seo_meta(const string& title, const string& desc) {
    return "<title>موقع ضربة شاكوش</title>"
           "<link rel='icon' type='image/jpeg' href='https://media.darbat-shakosh.com/channels4_profile%20(1).jpg'>"
           "<meta name='description' content='" + desc + "'>"
           "<meta name='keywords' content='حاسبة مقاسات المصاعد, كورس كهرباء المصاعد, تصفية أبعاد بئر المصعد, صيانة المصاعد, ميكانيكا المصاعد, ضربة شاكوش, هندسة المصاعد'>"
           "<meta name='robots' content='index, follow'>";
}

static string get_navbar_html() {
    const string logo_url = "https://media.darbat-shakosh.com/channels4_profile%20(1).jpg"; 
    const string chevron_svg = "<svg class='chevron' viewBox='0 0 24 24'><path d='M7 10l5 5 5-5z'/></svg>";
    const string moon_icon = "<svg class='theme-moon' viewBox='0 0 24 24'><path d='M12.3 22h-.1c-5.5 0-10-4.5-10-10 0-4.8 3.5-8.9 8.2-9.8.6-.1 1.2.3 1.3.9.1.6-.2 1.2-.8 1.4-3.3 1-5.7 4-5.7 7.5 0 4.4 3.6 8 8 8 3.5 0 6.5-2.4 7.5-5.7.2-.6.8-.9 1.4-.8.6.1 1 .7.9 1.3-.9 4.7-5 8.2-9.8 8.2z'/></svg>";
    const string sun_icon = "<svg class='theme-sun' viewBox='0 0 24 24'><path d='M12 7c-2.8 0-5 2.2-5 5s2.2 5 5 5 5-2.2 5-5-2.2-5-5-5zm0-5c.6 0 1 .4 1 1v2c0 .6-.4 1-1 1s-1-.4-1-1V3c0-.6.4-1 1-1zm0 14c.6 0 1 .4 1 1v2c0 .6-.4 1-1 1s-1-.4-1-1v-2c0-.6.4-1 1-1zM4 11h2c.6 0 1 .4 1 1s-.4 1-1 1H4c-.6 0-1-.4-1-1s.4-1 1-1zm14 0h2c.6 0 1 .4 1 1s-.4 1-1 1h-2c-.6 0-1-.4-1-1s.4-1 1-1zM5.2 5.2c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0L5.2 6.6c-.4-.4-.4-1 0-1.4zm12 12c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0l-1.4-1.4c-.4-.4-.4-1 0-1.4zM7.6 16.4c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0l-1.4-1.4c-.4-.4-.4-1 0-1.4zm12-12c.4-.4 1-.4 1.4 0l1.4 1.4c.4.4.4 1 0 1.4s-1 .4-1.4 0l-1.4-1.4c-.4-.4-.4-1 0-1.4z'/></svg>";

    return "<nav class='navbar'>"
           "  <div class='nav-right'>"
           "    <a href='/' class='navbar-brand'><span class='brand-mark'><img src='" + logo_url + "' alt='لوجو ضربة شاكوش'></span><span>ضربة شاكوش </span></a>"
           "    <div class='nav-center desktop-only'>"
           "      <a href='/' class='nav-link'>الرئيسية</a>"
           "      <a href='/paths' class='nav-link'>مسارات التعلّم</a>"
           "      <a href='/blog' class='nav-link'>الشروحات والمقالات</a>"
           "      <a href='/calculator' class='nav-link'>الحاسبة الهندسية</a>"
           "      <details class='nav-dropdown'>"
           "        <summary>المزيد " + chevron_svg + "</summary>"
           "        <div class='dropdown-panel'>"
           "          <div class='dropdown-col'>"
           "            <a href='/contact'>اتصل بنا</a>"
           "            <a href='/support'>مركز الدعم</a>"
           "            <a href='/donate'>دعم المنصة</a>"
           "          </div>"
           "        </div>"
           "      </details>"
           "    </div>"
           "  </div>"
           "  <div class='nav-left'>"
           "    <button class='nav-icon' id='themeBtn' title='تغيير الوضع المضيء/الليلي'>" + moon_icon + sun_icon + "</button>"
           "    <a class='nav-icon' title='بحث الفيديوهات'><svg viewBox='0 0 24 24'><path d='M15.5 14h-.79l-.28-.27C15.41 12.59 16 11.11 16 9.5 16 5.91 13.09 3 9.5 3S3 5.91 3 9.5 5.91 16 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z'/></svg></a>"
           "    <a class='nav-icon' title='بوابة المشتركين'><svg viewBox='0 0 24 24'><path d='M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 3c1.66 0 3 1.34 3 3s-1.34 3-3 3-3-1.34-3-3 1.34-3 3-3zm0 14.2c-2.5 0-4.71-1.28-6-3.22.03-1.99 4-3.08 6-3.08 1.99 0 5.97 1.09 6 3.08-1.29 1.94-3.5 3.22-6 3.22z'/></svg></a>"
           "    <details class='nav-dropdown mobile-menu mobile-only'>"
           "      <summary class='nav-icon' title='القائمة'><svg viewBox='0 0 24 24'><path d='M3 6h18v2H3zm0 5h18v2H3zm0 5h18v2H3z'/></svg></summary>"
           "      <div class='mobile-panel'>"
           "        <a href='/'>الرئيسية</a>"
           "        <a href='/paths'>مسارات التعلّم</a>"
           "        <a href='/blog'>الشروحات والمقالات</a>"
           "        <a href='/calculator'>الحاسبة الهندسية</a>"
           "        <div class='mobile-divider'></div>"
           "        <a href='/contact'>اتصل بنا</a>"
           "        <a href='/support'>مركز الدعم</a>"
           "        <a href='/donate'>دعم المنصة</a>"
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

    // 1️⃣ الصفحة الرئيسية
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        string nonce = generate_nonce(); set_csp(res, nonce);
        string meta = get_seo_meta("المنصة التعليمية والهندسية الأولى للمصاعد", "شروحات فنية متخصصة في ميكانيكا وكهرباء المصاعد وحساب أبعاد الصاعدة هندسياً.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() +
                      "</head><body>"
                      + get_navbar_html() +
                      "<div class='container'>"
                      "<div class='section-intro' style='text-align: center; margin-bottom: 40px;'>"
                      "  <h1 style='font-size: 2.2rem;'>مرحباً بك في منصة ضربة شاكوش الرقمية</h1>"
                      "  <p style='color: var(--text-muted); font-size: 1.1rem;'>الأدوات الهندسية الذكية والكورسات التطبيقية المتخصصة في مجال تركيب وصيانة المصاعد.</p>"
                      "</div>"
                      "<div class='grid-nav'>"
                      "  <a href='/calculator' class='nav-card'><h3>🛗 حاسبة المقاسات الهندسية</h3><p>ابدأ تصفية أبعاد البئر فوراً وحساب المقاسات الصافية للكابينة وثقل الموازنة بضغطة واحدة وبشكل معتمد.</p></a>"
                      "  <a href='/paths' class='nav-card'><h3>🧭 مسارات الكورسات والتعلم</h3><p>اكتشف مسار التأسيس ميكانيكياً، وكورس كهرباء المصاعد الشامل لتوصيل اللوحات والكنترولات خطوة بخطوة.</p></a>"
                      "</div>"
                      "</div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 2️⃣ واجهة الحاسبة
    svr.Get("/calculator", [](const httplib::Request&, httplib::Response& res) {
        string nonce = generate_nonce(); set_csp(res, nonce);
        string meta = get_seo_meta("حاسبة مقاسات بئر ومقصورة المصاعد", "أداة هندسية لحساب وتصفية مقاسات كابينة المصعد وأبعاد الثقل ونوع الأبواب المتاحة أوتوماتيكياً.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() +
                      "</head><body>"
                      + get_navbar_html() +
                      "<div class='container' style='max-width:650px;'>"
                      "<div class='card'><h2>🧮 حاسبة مقاسات بئر المصعد الفنية</h2>"
                      "<div class='sub-title'>الرجاء إدخال القياسات الحُرّة الصافية المأخوذة من الموقع للبدء في الحساب والتصفية التلقائية للمقايسة المعمارية:</div>"
                      "<form action='/calculate' method='post'>"
                      "<div class='f-group'><label>👑 نوع نظام تشغيل المصعد:</label>"
                      "<select name='m_type'>"
                      "<option value='MR'>غرفة محرك أعلى البئر القياسي (MR)</option>"
                      "<option value='MRL'>نظام بدون غرفة محرك علوية (MRL)</option>"
                      "<option value='Hydraulic'>نظام تشغيل هيدروليك (Hydraulic) 🛠️</option>"
                      "</select></div>"
                      "<div class='f-group'><label>📐 عرض بئر المصعد الحُر الصافي (CM):</label><input type='number' name='width' required min='80' max='250' placeholder='مثال لعرض البئر الحُر: 160'></div>"
                      "<div class='f-group'><label>📏 عمق بئر المصعد الحُر الصافي (CM):</label><input type='number' name='depth' required min='80' max='250' placeholder='مثال لعمق البئر الحُر: 160'></div>"
                      "<div class='f-group'><label>🏢 إجمالي عدد الوقفات (الأدوار الإنشائية):</label><input type='number' name='floors' required min='1' max='60' placeholder='أدخل عدد طوابق المبنى'></div>"
                      "<div class='f-group'><label>🕳️ عمق حفرة المصعد السفلية Pit (CM):</label><input type='number' name='depth_pit' required min='10' max='500' value='100'></div>"
                      "<div class='f-group'><label>🏠 ارتفاع الدور الأخير من بلاطة الوقف للجريد Overhead (CM):</label><input type='number' name='overhead' required min='100' max='800' value='400'></div>"
                      "<div class='f-group'><label>⛓️ حساب الخامات والبضاعة التقريبية:</label><select name='calc_mat'><option value='yes'>نعم، أظهر جدول البضاعة والسكك المطلوبة كاملة</option><option value='no'>لا، أريد مقاسات البئر فقط</option></select></div>"
                      "<button type='submit'>🏛️ استخراج مقاسات الصاعدة الهندسية وتوليد المقايسة</button></form>"
                      "</div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 3️⃣ معالجة الحسابات وتوليد الجداول الثلاثية الشاملة المفككة بالكامل
    svr.Post("/calculate", [&elevator](const httplib::Request& req, httplib::Response& res) {
        string m_type = html_escape(req.get_param_value("m_type"));
        if (m_type != "MR" && m_type != "MRL" && m_type != "Hydraulic") m_type = "MR";

        if (m_type == "MR" && !IS_MR_READY) {
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                          + get_modern_blue_css() + "</head><body>"
                          "<div style='display:flex; align-items:center; justify-content:center; min-height:100vh;'>"
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
                          "<div style='display:flex; align-items:center; justify-content:center; min-height:100vh;'>"
                          "<div class='card' style='border-color:var(--accent-2); max-width:520px; text-align:center;'>"
                          "<h2>⚙️ نظام MRL قيد التعديل</h2>"
                          "<p style='color:var(--text-muted); margin-bottom:24px; line-height:1.7;'>جاري تعديل وتحديث معادلات المصاعد الجيرليس (بدون غرفة) حالياً من قِبل محمد الشعراوي.. شكراً لثقتكم!</p>"
                          "<a class='btn-secondary' href='/calculator'>🔄 العودة للحاسبة</a>"
                          "</div></div></body></html>";
            res.set_content(html, "text/html; charset=utf-8"); return;
        }

        if (m_type == "Hydraulic" && !IS_HYDRAULIC_READY) {
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&display=swap' rel='stylesheet'>"
                          + get_modern_blue_css() + "</head><body>"
                          "<div style='display:flex; align-items:center; justify-content:center; min-height:100vh;'>"
                          "<div class='card' style='border-color:var(--accent-2); max-width:520px; text-align:center;'>"
                          "<h2>⚙️ النظام الهيدروليكي قيد التطوير</h2>"
                          "<p style='color:var(--text-muted); margin-bottom:24px; line-height:1.7;'>جاري العمل حالياً على تدقيق وضبط خوارزميات حسابات المصاعد الهيدروليكية وخاماتها.. شكراً لثقتكم ومنتظرينكم قريباً جداً!</p>"
                          "<a class='btn-secondary' href='/calculator'>🔄 العودة للحاسبة</a>"
                          "</div></div></body></html>";
            res.set_content(html, "text/html; charset=utf-8"); return;
        }

        string calc_mat = html_escape(req.get_param_value("calc_mat"));

        int w = safe_stoi(req.get_param_value("width"), 0);
        int d = safe_stoi(req.get_param_value("depth"), 0);
        float f = safe_stof(req.get_param_value("floors"), 0.0f);
        int p = safe_stoi(req.get_param_value("depth_pit"), 100);
        int oh = safe_stoi(req.get_param_value("overhead"), 400);

        if (w < 110 || d < 100 || f <= 0 || p <= 0 || oh <= 0 || w > 250 || d > 250) {
            string err = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                         "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                         + get_modern_blue_css() + "</head><body>"
                         "<div style='display:flex; align-items:center; justify-content:center; min-height:100vh;'>"
                         "<div class='card' style='border-color:#ef4444; max-width:500px; text-align:center;'>"
                         "<h2>⚠️ البيانات المدخلة غير سليمة هندسياً</h2>"
                         "<p style='color:#94a3b8;'>يرجى إدخال قيم موجبة مابين 110سم إلى 250سم كحد أقصى لعرض وعمق البئر الصافي.</p>"
                         "<a href='/calculator' class='btn-action' style='background:#ef4444;'>🔄 العودة وتعديل أبعاد البئر</a>"
                         "</div></div>"
                         "</body></html>";
            res.set_content(err, "text/html; charset=utf-8"); return;
        }

        string door = elevator.get_door_type(w);
        int cabin_dbg = elevator.get_cabin_dbg(w);
        int cwt_dbg = elevator.get_cwt_dbg(w);
        int cab_w = elevator.get_cabin_width(w);
        int cab_d = elevator.get_cabin_depth(d);
        float h = elevator.get_shaft_height(f, p/100.0f, oh/100.0f, m_type);

        Elevator::FullSpecificationReport specs = elevator.compile_full_specification(w, d, static_cast<int>(f), m_type);

        string nonce = generate_nonce(); set_csp(res, nonce);
        ostringstream os;
        os << "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
           "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
           "<script src='https://cdnjs.cloudflare.com/ajax/libs/html2pdf.js/0.10.1/html2pdf.bundle.min.js'></script>"
           + get_modern_blue_css() + "</head><body>"
           << get_navbar_html()
           << "<div class='container' style='max-width:780px;'>"
           << "<div class='card' id='pdf-area'><h2>📋 تقرير المقايسة وتصفية المقاسات الفنية</h2>"
           << "<div class='sub-title' style='margin-bottom:20px;'>المقايسة المعتمدة هندسياً من منصة ضربة شاكوش الرقمية لتوريد وتركيب الخامات:</div>"
           
           << "<h3 style='color:var(--accent); font-size:1.15rem; margin-bottom:10px;'>📌 أولاً: المقاسات الإنشائية وأبعاد الصاعدة:</h3>"
           << "<div class='table-container'><table class='tbl'>"
           << "<tr><th>نوع ونظام التشغيل:</th><td>" << (m_type == "MR" ? "غرفة محرك علوية القياسي" : (m_type == "MRL" ? "بدون غرفة محرك جيرليس MRL" : "نظام بستم هيدروليك Hydraulic 🛠️")) << "</td></tr>"
           << "<tr><th>أبعاد البئر الحرة المأخوذة:</th><td>العرض: " << w << " سم || العمق: " << d << " سم</td></tr>"
           << "<tr><th>نوع وعرض الأبواب المتاحة للبئر:</th><td style='color:#38bdf8; font-weight:700;'>[" << door << "]</td></tr>"
           << "<tr><th>مقاس شاسيه DBG الكابينة الصافي:</th><td>" << cabin_dbg << " CM</td></tr>"
           << "<tr><th>مقاس شاسيه DBG ثقل الموازنة:</th><td>" << (m_type == "Hydraulic" ? "بدون ثقل" : to_string(cwt_dbg) + " CM") << "</td></tr>"
           << "<tr><th>صافي الأبعاد الداخلية لمقصورة الركاب:</th><td style='color:#f5a524;'>العرض: " << cab_w << " سم × العمق: " << cab_d << " سم</td></tr>"
           << "<tr><th>حمولة الماكينة المقدرة والمقترحة هندسياً:</th><td style='color:#16a34a; font-weight:800;'>" << specs.rated_load_kg << " كجم (المطابقة لجدول EN 81-20)</td></tr>"
           << "<tr><th>إجمالي مشوار البئر والارتفاع الرأسي:</th><td>" << h << " متر طولي</td></tr>"
           << "</table></div>";

        if (calc_mat == "yes") {
            // تفكيك وعرض كل بند منفرد بخلية مستقلة مع عدده بدقة تامة
            os << "<h3 style='color:var(--accent); font-size:1.15rem; margin-top:25px; margin-bottom:10px;'>⚙️ ثانياً: بضاعة المرحلة الأولى (السكك والأبواب والحديد):</h3>"
               << "<div class='table-container'><table class='tbl'>"
               << "<thead><tr><th>اسم بيان الصنف والخامة الحديدية</th><th>الكمية المطلوبة للموقع</th></tr></thead>"
               << "<tbody>"
               << "<tr><td>" << specs.scaffolding_name << "</td><td>" << specs.scaffolding_count << " طقم متكامل</td></tr>"
               << "<tr><td>" << specs.plumb_name << "</td><td>" << specs.plumb_count << " مجموعة عيار</td></tr>"
               << "<tr><td>" << specs.balance_name << "</td><td>" << specs.balance_count << " قطعة ميزان</td></tr>"
               << "<tr><td>" << specs.door_exterior_name << "</td><td>" << specs.total_exterior_doors << " باب خارجي</td></tr>"
               << "<tr><td>" << specs.cabin_rails_name << "</td><td>" << specs.cabin_rails_count << " عود سكة</td></tr>"
               << "<tr><td>" << specs.cwt_rails_name << "</td><td>" << specs.cwt_rails_count << " عود سكة</td></tr>"
               << "<tr><td>" << specs.cabin_brackets_name << "</td><td>" << specs.cabin_brackets_count << " كابول تجميع</td></tr>"
               << "<tr><td>" << specs.cwt_brackets_name << "</td><td>" << specs.cwt_brackets_count << " كابول تجميع</td></tr>"
               << "<tr><td>" << specs.sub_cabin_name << "</td><td>" << specs.sub_cabin_count << " قطعة تدعيم</td></tr>"
               << "<tr><td>" << specs.sub_cwt_name << "</td><td>" << specs.sub_cwt_count << " قطعة تدعيم</td></tr>"
               << "<tr><td>" << specs.hilti_bolts_name << "</td><td>" << specs.hilti_bolts_12mm << " مسمار هلتي</td></tr>"
               << "<tr><td>" << specs.assembly_bolts_name << "</td><td>" << specs.assembly_bolts_12mm << " مسمار عادي</td></tr>"
               << "<tr><td>" << specs.bolts_8mm_name << "</td><td>" << specs.bolts_8mm << " مسمار تجميع ثقل</td></tr>"
               << "<tr><td>" << specs.spring_washers_8mm_name << "</td><td>" << specs.spring_washers_8mm << " طقم صامولة ووردة</td></tr>"
               << "<tr><td>" << specs.spring_washers_12mm_name << "</td><td>" << specs.spring_washers_12mm << " وردة سوستة أمان</td></tr>"
               << "<tr><td>" << specs.nuts_12mm_name << "</td><td>" << specs.nuts_12mm << " صامولة 12 مجلفنة</td></tr>"
               << "<tr><td>" << specs.flat_washers_12mm_name << "</td><td>" << specs.flat_washers_12mm << " وردة صاج عريضة</td></tr>"
               << "<tr><td>" << elevator.get_welding_rods() << "</td><td>حسب الاستهلاك الدوري</td></tr>"
               << "<tr><td>" << elevator.get_grinding_discs() << "</td><td>حسب الاستهلاك الدوري</td></tr>"
               << "<tr><td>📌 ملاحظات التركيب وحلوق الأبواب:</td><td style='font-size:0.9rem; color:var(--text-muted);'>" << specs.door_casing_note << "</td></tr>"
               << "</tbody></table></div>";

            os << "<h3 style='color:var(--accent); font-size:1.15rem; margin-top:25px; margin-bottom:10px;'>⚡ ثالثاً: بضاعة المرحلة الثانية والثالثة (المحرك والمقصورة والكهرباء):</h3>"
               << "<div class='table-container'><table class='tbl'>"
               << "<thead><tr><th>اسم بيان المكون الميكانيكي والكهربائي</th><th>الكمية المطلوبة للموقع</th></tr></thead>"
               << "<tbody>"
               << "<tr><td>" << specs.ceiling_cut_name << "</td><td>" << specs.ceiling_cut_count << " فتحة هندسية</td></tr>"
               << "<tr><td>مسامير 24 مللي لتثبيت المحرك</td><td>" << specs.machine_bolts_24mm << " مسامير عيار ثقيل</td></tr>"
               << "<tr><td>نوع المحرك والمقاس المعتمد للسيستم</td><td>" << specs.machine_type_desc << "</td></tr>"
               << "<tr><td>قواعد امتصاص الاهتزازات الفنية</td><td>" << specs.machine_rubber_note << "</td></tr>"
               << "<tr><td>نوع وتصميم شواكيل المقصورة (دوال)</td><td>" << specs.cabin_design_type << "</td></tr>"
               << "<tr><th>موضع ونظام شواكيل ثقل الموازنة (دوال)</th><td>" << specs.cwt_design_type << "</td></tr>"
               << "<tr><td>أطوال حبال الجر (الويرات) المطلوبة</td><td>" << specs.cabin_wires_meters << " متر طولي [ " << specs.cabin_wires_name << " ]</td></tr>"
               << "<tr><td>شدادات حبل معتمدة عيار الحبل</td><td>" << specs.rope_hitches_count << " شداد فولاذي</td></tr>"
               << "<tr><td>زراجين حبل حديد للأمان وعقد الوير</td><td>" << specs.rope_clamps_count << " زرجينة حبل</td></tr>"
               << "<tr><td>" << specs.parachute_name << "</td><td>" << specs.parachute_count << " جهاز براشوت سفلي</td></tr>"
               << "<tr><td>" << specs.governor_rope_name << "</td><td>" << specs.governor_rope_meters << " متر طولي حبل</td></tr>"
               << "<tr><td>" << specs.buffer_set_name << "</td><td>" << specs.buffer_set_count << " طقم هيدروليكي موثوق</td></tr>"
               << "<tr><td>طارات المناول للموجّه بالبئر</td><td>" << elevator.get_deflector_sheaves(m_type) << " طارة مناول هندسية</td></tr>"
               << "<tr><td>" << specs.cwt_blocks_weight_desc << "</td><td>" << specs.counterweight_blocks << " كجم بلوك زهر</td></tr>"
               << "<tr><td>" << specs.control_panel_name << "</td><td>" << specs.control_panel_count << " لوحة تحكم رئيسية</td></tr>"
               << "<tr><td>" << specs.ard_system_name << "</td><td>" << specs.ard_system_count << " جهاز إنقاذ آلي</td></tr>"
               << "<tr><td>" << specs.ctrl_fischer_name << "</td><td>" << specs.ctrl_fischer_count << " طقم مسامير فيشر</td></tr>"
               << "<tr><td>" << specs.inspect_box_name << "</td><td>" << specs.inspect_box_count << " علبة صيانة علوية</td></tr>"
               << "<tr><td>" << specs.charger_batt_name << "</td><td>" << specs.charger_batt_count << " شاحن وبطارية</td></tr>"
               << "<tr><td>" << specs.emerg_alarm_name << "</td><td>" << specs.emerg_alarm_count << " جرس وسارينة طوارئ</td></tr>"
               << "<tr><td>" << specs.flex_cable_name << "</td><td>" << specs.flex_cable_meters << " متر كيبل مرن للمتحرك</td></tr>"
               << "<tr><td>" << specs.flex_holder_name << "</td><td>" << specs.flex_holders_count << " حوامل تثبيت حديدية</td></tr>"
               << "<tr><td>" << specs.trunk_4cm_name << "</td><td>" << specs.trunk_4cm_meters << " متر ترنكات بلاستيك</td></tr>"
               << "<tr><td>" << specs.trunk_10cm_name << "</td><td>" << specs.static_trunk_10cm << " متر ترنكات تمديد</td></tr>"
               << "<tr><td>" << specs.trunk_screws_name << "</td><td>" << specs.trunk_screws_8mm << " مسمار وفيشر 8 مللي</td></tr>"
               << "<tr><td>" << specs.oiler_set_name << "</td><td>" << specs.oiler_set_count << " طقم مزايت للسكك</td></tr>"
               << "<tr><td>" << specs.wire_6mm_name << "</td><td>" << specs.wire_6mm_count << " لفة رئيسية للباور</td></tr>"
               << "<tr><td>" << specs.wire_1mm_name << "</td><td>" << specs.wire_1mm_count_desc << " لإشارات الدوائر</td></tr>"
               << "<tr><td>" << specs.net_cable_name << "</td><td>" << specs.net_cable_meters << " متر تمديد شاشات</td></tr>"
               << "<tr><td>" << specs.cop_panel_name << "</td><td>" << specs.cop_panel_count << " لوحة طلبات مقصورة</td></tr>"
               << "<tr><td>" << specs.lop_buttons_name << "</td><td>" << specs.lop_buttons_count << " لوحة أدوار خارجية</td></tr>"
               << "<tr><td>" << specs.intercom_name << "</td><td>" << specs.intercom_count << " جهاز اتصال داخلي</td></tr>"
               << "<tr><td>" << specs.safety_door_name << "</td><td>" << specs.safety_door_count << " بوابة حماية داخلية</td></tr>"
               << "<tr><td>" << specs.photocell_name << "</td><td>" << specs.photocell_count << " ستارة أمان ضوئية</td></tr>"
               << "<tr><td>" << specs.stop_magnet_name << "</td><td>" << specs.stop_magnet_count << " شريط مغناطيس توقف</td></tr>"
               << "<tr><td>" << specs.count_magnet_name << "</td><td>" << specs.count_magnet_count << " شريط مغناطيس عداد</td></tr>"
               << "<tr><td>3 ليميت سويتش ميكانيكي بالبئر</td><td>ثابتة (3 قطع ليميت أمان)</td></tr>"
               << "<tr><td>3 حساسات مغناطيسية عيارية</td><td>ثابتة (3 قطع حساس لفل)</td></tr>"
               << "<tr><td>" << specs.floor_poles_name << "</td><td>" << specs.floor_poles_count << " قطعة بول مغناطيسي للسكك</td></tr>"
               << "<tr><td>لقم وكراسي تزليق المقصورة (لقم)</td><td>4 كراسي وتزليق كامل للكابينة</td></tr>"
               << "<tr><td>لقم وكراسي تزليق وزن الثقل (لقم)</td><td>" << (m_type == "Hydraulic" ? "0 كراسي" : "4 كراسي وتزليق للوزن") << "</td></tr>"
               << "<tr><td>📝 لوجيك تأمين الستارة والأمان الضوئي:</td><td style='font-size:0.88rem; color:var(--text-muted);'>" << specs.photocell_note << "</td></tr>"
               << "</tbody></table></div>";
        }

        os << "</div>" 
           << "<div class='actions' style='max-width:750px; margin: 20px auto 0 auto; padding:0 40px;'>"
           << "  <button class='btn-print' id='pBtn'>📥 تحميل التقرير والمقايسة كملف PDF</button>"
           << "  <a class='btn-secondary' href='/calculator'>🔄 تصفية مقاسات بئر جديد</a>"
           << "</div></div>"
           << "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
           << "<script nonce='" << nonce << "'>"
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
           "  document.getElementById('pBtn').addEventListener('click', function(){"
           "    var element = document.getElementById('pdf-area');"
           "    var opt = {"
           "      margin:        [0.3, 0.3, 0.3, 0.3],"
           "      filename:      'Hammer_Elevator_Full_Specification.pdf',"
           "      image:         { type: 'jpeg', quality: 1.0 },"
           "      html2canvas:   { scale: 2, useCORS: true, backgroundColor: document.body.classList.contains('light-mode') ? '#ffffff' : '#121826', scrollY: 0 },"
           "      jsPDF:         { unit: 'in', format: 'a4', orientation: 'portrait' }"
           "    };"
           "    html2pdf().set(opt).from(element).save();"
           "  });"
           << "</script>"
           << "</body></html>";
        res.set_content(os.str(), "text/html; charset=utf-8");
    });

    // 4️⃣ صفحة مكتبة المقالات والدروس
    svr.Get("/blog", [](const httplib::Request&, httplib::Response& res) {
        string nonce = generate_nonce(); set_csp(res, nonce);
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
                      + get_navbar_html() +
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

    // 4.1️⃣ تفاصيل مقال أو فيديو واحد بالتفصيل
    svr.Get(R"(/lesson/([a-zA-Z0-9\-]+))", [](const httplib::Request& req, httplib::Response& res) {
        string nonce = generate_nonce(); set_csp(res, nonce);
        string slug = req.matches[1].str();
        auto lessons = get_lessons();
        auto it = find_if(lessons.begin(), lessons.end(), [&](const Lesson& l) { return l.slug == slug; });

        if (it == lessons.end()) {
            res.status = 404;
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          + get_modern_blue_css() + "</head><body>"
                          + get_navbar_html() +
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
                      + get_navbar_html() +
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

    // 4.2️⃣ صفحة المسارات التعلّمية
    svr.Get("/paths", [](const httplib::Request&, httplib::Response& res) {
        string nonce = generate_nonce(); set_csp(res, nonce);
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
                      + get_navbar_html() +
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

    // 4.3️⃣ صفحة محتويات مسار واحد
    svr.Get(R"(/track/([a-zA-Z0-9\-]+))", [](const httplib::Request& req, httplib::Response& res) {
        string nonce = generate_nonce(); set_csp(res, nonce);
        string slug = req.matches[1].str();
        auto tracks = get_tracks();
        auto trackIt = find_if(tracks.begin(), tracks.end(), [&](const Track& t) { return t.slug == slug; });

        if (trackIt == tracks.end()) {
            res.status = 404;
            string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                          + get_modern_blue_css() + "</head><body>"
                          + get_navbar_html() +
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
                      + get_navbar_html() +
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

    // 5️⃣ صفحة اتصل بنا
    svr.Get("/contact", [](const httplib::Request&, httplib::Response& res) {
        string nonce = generate_nonce(); set_csp(res, nonce);
        string meta = get_seo_meta("اتصل بنا | الدعم الفني", "تواصل مباشرة مع إدارة منصة ضربة شاكوش لطرح الأسئلة الفنية أو الإبلاغ عن مشكلة برمجية.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html() +
                      "<div class='container' style='max-width:650px;'>"
                      "<div class='card'><h2>📩 تواصل مع الدعم الفني للهندسة</h2>"
                      "<div class='sub-title'>لديك أي استفسار فني خاص بالمقاسات، اقتراح لتطوير الموقع، أو واجهتك مشكلة بالحاسبة؟ تواصل معنا فوراً.</div>"
                      "<a class='btn-action' href='mailto:support@darbat-shakosh.com' style='margin-bottom:14px; display:block;'>📧 راسلنا على البريد الإلكتروني الرسمي للمنصة</a>"
                      "<p style='color:#8b96ab; font-size:0.9rem; text-align:center;'>المراسلات يتم الرد عليها ومراجعتها من المهندس المختص خلال 24 ساعة.</p>"
                      "</div></div>"
                      "<div class='footer'>منصة ضربة شاكوش الفنية © 2026 - إنشاء محمد الشعراوي</div>"
                      + get_theme_script(nonce) +
                      "</body></html>";
        res.set_content(html, "text/html; charset=utf-8");
    });

    // 6️⃣ مركز المساعدة
    svr.Get("/support", [](const httplib::Request&, httplib::Response& res) {
        string nonce = generate_nonce(); set_csp(res, nonce);
        string meta = get_seo_meta("مركز المساعدة والأسئلة الشائعة الفنية للمصاعد.", "محتاج مساعدة في فهم كيفية حساب أبعاد الـ DBG الصافي وشواكيل التصفية؟");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html() +
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

    // 7️⃣ صفحة دعم واستمرارية المنصة
    svr.Get("/donate", [](const httplib::Request&, httplib::Response& res) {
        string nonce = generate_nonce(); set_csp(res, nonce);
        string meta = get_seo_meta("الموقع وحاسبة مقاسات بئر المصاعد مجاني تماماً لخدمة الوطن العربي.", "الموقع وحاسبة مقاسات بئر المصاعد مجاني تماماً لخدمة فنيي ومندوبي ومهندسي الوطن العربي.");
        string html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                      "<link href='https://fonts.googleapis.com/css2?family=Cairo:wght@400;600;700;800&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
                      + meta + get_modern_blue_css() + "</head><body>"
                      + get_navbar_html() +
                      "<div class='container' style='max-width:650px;'>"
                      "<div class='card'><h2>❤️ المساهمة في دعم واستمرارية المنصة</h2>"
                      "<div class='sub-title'>الموقع وحاسبة مقاسات بئر المصاعد مجاني تماماً لخدمة فنيي ومندوبي ومهندسي الوطن العربي، مساهمتك تساعد في تطوير الخوادم وزيادة جودة الشروحات الميكانيكية.</div>"
                      "<a class='btn-action' href='/contact' style='display:block;'>💳 استعراض وسائل وطرق المساهمة المتاحة</a>"
                      "</div></div>"
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
