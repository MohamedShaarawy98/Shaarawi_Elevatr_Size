# استخدام نسخة خفيفة ومستقرة من دبيان تحتوي على أدوات البناء
FROM gcc:latest AS builder

# تحديد مجلد العمل داخل الحاوية
WORKDIR /app

# نسخ ملفات المشروع بالكامل إلى الحاوية
COPY . .

# عمل Compile لكود الـ ++C وربطه بالمكتبات المطلوبة
RUN g++ -O3 main.cpp -o hammer_platform -pthread

# استخدام حاوية تشغيل نظيفة وخفيفة جداً لتقليل المساحة وتسريع الأداء
FROM debian:bookworm-slim

WORKDIR /app

# تثبيت حزم الشهادات الأمنية لضمان عمل روابط الـ HTTPS (مثل flagcdn و media) بدون مشاكل
RUN apt-get update && apt-get install -y ca-certificates && rm -rf /var/lib/apt/lists/*

# نسخ الملف التنفيذي فقط من المرحلة الأولى
COPY --from=builder /app/hammer_platform .

# إعلام الحاوية بالمنفذ الذي سيعمل عليه السيرفر
EXPOSE 8080

# أمر تشغيل السيرفر فوراً عند انطلاق الحاوية
CMD ["./hammer_platform"]
