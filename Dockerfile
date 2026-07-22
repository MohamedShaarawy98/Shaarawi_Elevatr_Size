FROM gcc:latest

WORKDIR /app

# تحديث النظام وتطيب مكتبات الـ OpenSSL بشكل كامل
RUN apt-get update && apt-get install -y \
    libssl-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

COPY . .

# بناء الكود مع تحديد المسارات الواضحة للمكتبات
RUN g++ -std=c++17 main.cpp -o server -lssl -lcrypto -lpthread

EXPOSE 8080

CMD ["./server"]
