# استخدام نفس نسخة دبيان المستقرة لبيئتي البناء والتشغيل لضمان توافق المكتبات
FROM debian:bookworm-slim AS builder

# تثبيت أدوات بناء الـ ++C المطلوبة داخل بيئة دبيان
RUN apt-get update && apt-get install -y \
    g++ \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

# عمل Compile لكود الـ ++C وربطه بالمكتبات
RUN g++ -O3 main.cpp -o hammer_platform -pthread

# مرحلة التشغيل النهائية النظيفة
FROM debian:bookworm-slim

WORKDIR /app

# تثبيت حزم الشهادات الأمنية
RUN apt-get update && apt-get install -y ca-certificates && rm -rf /var/lib/apt/lists/*

# نسخ الملف التنفيذي من مرحلة البناء بنفس التوافقية
COPY --from=builder /app/hammer_platform .

EXPOSE 8080

CMD ["./hammer_platform"]
