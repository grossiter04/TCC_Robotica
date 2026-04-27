#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <fcntl.h>   
#include <termios.h> 
#include <unistd.h>  

const std::string CONFIG_PATH = "/home/gabriel/Desktop/TCC/raspberry/buzzline_config.json";

struct PdConfig {
    float kp = 0.5f;
    float kd = 0.0f;
    int   velocidade_base = 130;
    int   vel_min = 100;
    int   vel_max = 255;
    int   erro_limiar_curva = 120; // Adicionado aqui
};

float extrairFloat(const std::string& json, const std::string& chave) {
    std::string busca = "\"" + chave + "\"";
    size_t pos = json.find(busca);
    if (pos == std::string::npos) return -1;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return -1;
    return std::stof(json.substr(pos + 1));
}

PdConfig lerConfig() {
    PdConfig cfg;
    std::ifstream f(CONFIG_PATH);
    if (!f.is_open()) return cfg;
    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    try {
        float kp = extrairFloat(json, "kp");
        float kd = extrairFloat(json, "kd");
        float vb = extrairFloat(json, "velocidade_base");
        float vm = extrairFloat(json, "vel_min");
        float el = extrairFloat(json, "erro_limiar_curva"); // Lendo do JSON
        if (kp >= 0) cfg.kp = kp;
        if (kd >= 0) cfg.kd = kd;
        if (vb >= 0) cfg.velocidade_base = (int)vb;
        if (vm >= 0) cfg.vel_min = (int)vm;
        if (el >= 0) cfg.erro_limiar_curva = (int)el;
    } catch (...) {}
    return cfg;
}

// ... (Funções de Serial e Contorno permanecem as mesmas) ...
int abrirPortaSerial(const char* portname) {
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) return -1;
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) return -1;
    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_lflag = 0; tty.c_oflag = 0;
    if (tcsetattr(fd, TCSANOW, &tty) != 0) return -1;
    return fd;
}

void enviarVelocidades(int serial_port, int velEsq, int velDir) {
    std::string comando = std::to_string(velEsq) + "," + std::to_string(velDir) + "\n";
    write(serial_port, comando.c_str(), comando.length());
}

int encontrarMaiorContorno(const std::vector<std::vector<cv::Point>>& contours) {
    if (contours.empty()) return -1;
    double maxArea = 0.0; int idx = -1;
    for (int i = 0; i < (int)contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area > maxArea) { maxArea = area; idx = i; }
    }
    return idx;
}

int main() {
    int serial_port = abrirPortaSerial("/dev/ttyUSB0");
    if (serial_port < 0) return -1;

    std::string pipeline =
    "libcamerasrc ! "
    "video/x-raw,format=NV21,width=640,height=480,framerate=30/1 ! "
    "videoconvert ! "
    "video/x-raw,format=BGR ! "
    "appsink drop=true max-buffers=1 sync=false";
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened()) return -1;

    PdConfig cfg = lerConfig();
    int erro_anterior = 0;
    int frame_counter = 0;

    while (true) {
        if (frame_counter % 30 == 0) { cfg = lerConfig(); }
        frame_counter++;

        cv::Mat frame;
        if (!cap.read(frame)) {
            std::cerr << "[ERRO] Falha ao ler frame da camera!" << std::endl;
            break;
        }

        std::cout << "[OK] Frame " << frame_counter << " lido." << std::endl;

        if (!cap.read(frame)) break;

        int width = frame.cols;
        int height = frame.rows;
        cv::Rect roi_rect(0, height * 0.6, width, height * 0.4);
        cv::Mat roi = frame(roi_rect);

        cv::Mat gray, blurred, thresh;
        cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, blurred, cv::Size(7, 7), 0);
        cv::threshold(blurred, thresh, 150, 255, cv::THRESH_BINARY);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

        int largestIdx = encontrarMaiorContorno(contours);
        int velEsq = 0, velDir = 0;

        if (largestIdx != -1) {
            cv::Moments M = cv::moments(contours[largestIdx]);
            if (M.m00 != 0) {
                int cx = (int)(M.m10 / M.m00);
                int erro = (width / 2) - cx;
                int derivada = erro - erro_anterior;
                float ajuste = (cfg.kp * erro) + (cfg.kd * derivada);

                velEsq = std::max(cfg.vel_min, std::min(cfg.vel_max, (int)(cfg.velocidade_base - ajuste)));
                velDir = std::max(cfg.vel_min, std::min(cfg.vel_max, (int)(cfg.velocidade_base + ajuste)));

                // --- Lógica de Travamento Dinâmica ---
                if (erro > cfg.erro_limiar_curva) velEsq = 0;
                else if (erro < -cfg.erro_limiar_curva) velDir = 0;

                erro_anterior = erro;
                enviarVelocidades(serial_port, velEsq, velDir);
            }
        } else {
            enviarVelocidades(serial_port, 0, 0);
            erro_anterior = 0;
        }

        if (cv::waitKey(30) == 'q') break;
    }
    close(serial_port);
    return 0;
}