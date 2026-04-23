#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>       // Para leitura do arquivo de config
#include <opencv2/opencv.hpp>
#include <fcntl.h>   
#include <termios.h> 
#include <unistd.h>  

// --- CAMINHO DO ARQUIVO DE CONFIG (compartilhado com server.py) ---
const std::string CONFIG_PATH = "/home/gabriel/Desktop/TCC/raspberry/buzzline_config.json";

// --- STRUCT DE CONFIGURAÇÃO PD ---
struct PdConfig {
    float kp             = 0.5f;
    float kd             = 0.0f;
    int   velocidade_base = 130;
    int   vel_min        = 100;
    int   vel_max        = 255;
};

// --- PARSER JSON MINIMALISTA ---
// Lê apenas valores simples do tipo "chave": valor (sem dependência de lib externa)
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
    if (!f.is_open()) return cfg; // Retorna padrões se não encontrar o arquivo

    std::string json((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());

    try {
        float kp = extrairFloat(json, "kp");
        float kd = extrairFloat(json, "kd");
        float vb = extrairFloat(json, "velocidade_base");
        float vm = extrairFloat(json, "vel_min");
        float vx = extrairFloat(json, "vel_max");

        if (kp >= 0) cfg.kp              = kp;
        if (kd >= 0) cfg.kd              = kd;
        if (vb >= 0) cfg.velocidade_base = (int)vb;
        if (vm >= 0) cfg.vel_min         = (int)vm;
        if (vx >= 0) cfg.vel_max         = (int)vx;
    } catch (...) {
        std::cerr << "[Config] Erro ao parsear JSON, usando valores anteriores." << std::endl;
    }

    return cfg;
}

// --- FUNÇÕES AUXILIARES PARA A SERIAL ---

int abrirPortaSerial(const char* portname) {
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        std::cerr << "Erro ao abrir a porta serial: " << portname << std::endl;
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "Erro ao pegar atributos da serial" << std::endl;
        return -1;
    }

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 5;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Erro ao setar atributos da serial" << std::endl;
        return -1;
    }
    return fd;
}

void enviarVelocidades(int serial_port, int velEsq, int velDir) {
    std::string comando = std::to_string(velEsq) + "," + std::to_string(velDir) + "\n";
    write(serial_port, comando.c_str(), comando.length());
}

// --- FIM DAS FUNÇÕES DA SERIAL ---

int encontrarMaiorContorno(const std::vector<std::vector<cv::Point>>& contours) {
    if (contours.empty()) return -1;
    double maxArea = 0.0;
    int largestContourIndex = -1;
    for (int i = 0; i < (int)contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            largestContourIndex = i;
        }
    }
    return largestContourIndex;
}

int main() {
    // --- CONFIGURAÇÃO DA PORTA SERIAL ---
    const char* porta_serial_nome = "/dev/ttyUSB0"; 
    
    int serial_port = abrirPortaSerial(porta_serial_nome);
    if (serial_port < 0) {
        std::cerr << "Nao foi possivel conectar ao Arduino. Verifique a porta." << std::endl;
        return -1; 
    }
    std::cout << "Conectado ao Arduino na porta " << porta_serial_nome << std::endl;

    // --- CONFIGURAÇÃO DA CÂMERA ---
    std::string pipeline = "libcamerasrc ! video/x-raw,width=640,height=480,framerate=30/1 ! videoconvert ! appsink";
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);

    if (!cap.isOpened()) {
        std::cerr << "Erro: Nao foi possivel abrir a camera." << std::endl;
        return -1;
    }

    // --- PARÂMETROS DE VISÃO (fixos — não mudam em tempo real) ---
    int limiar_threshold     = 150;      
    int tamanho_kernel_blur  = 7;     
    double altura_roi_percentual = 0.4;

    // --- CARREGA CONFIG INICIAL ---
    PdConfig cfg = lerConfig();
    std::cout << "[Config] Kp=" << cfg.kp 
              << " Kd=" << cfg.kd 
              << " Base=" << cfg.velocidade_base
              << " Min=" << cfg.vel_min << std::endl;

    // --- VARIÁVEIS DE CONTROLE ---
    int erro_anterior = 0;
    int ultima_vel_esq = -1;
    int ultima_vel_dir = -1;
    int frame_counter  = 0;  // Para releitura periódica do config

    std::cout << "Iniciando Controle PD. Pressione 'q' para sair." << std::endl;
    std::cout << "Ajuste os parametros em http://<IP-da-Raspberry>:5000" << std::endl;

    // --- CRIAÇÃO DAS JANELAS DE VISUALIZAÇÃO ---
    cv::namedWindow("Visao do Robo (Colorida)", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("Mascara (Preto e Branco)", cv::WINDOW_AUTOSIZE);

    // --- LOOP PRINCIPAL ---
    while (true) {
        // =========================================================
        // --- RELEITURA DO CONFIG A CADA 30 FRAMES (~1 segundo) ---
        // =========================================================
        if (frame_counter % 30 == 0) {
            PdConfig novo_cfg = lerConfig();

            // Detecta e loga mudanças
            if (novo_cfg.kp != cfg.kp || novo_cfg.kd != cfg.kd ||
                novo_cfg.velocidade_base != cfg.velocidade_base ||
                novo_cfg.vel_min != cfg.vel_min) {
                
                std::cout << "[Config Atualizado] "
                          << "Kp: " << cfg.kp << " -> " << novo_cfg.kp << " | "
                          << "Kd: " << cfg.kd << " -> " << novo_cfg.kd << " | "
                          << "Base: " << cfg.velocidade_base << " -> " << novo_cfg.velocidade_base << " | "
                          << "Min: " << cfg.vel_min << " -> " << novo_cfg.vel_min << std::endl;
                
                cfg = novo_cfg;
                // Zera o erro anterior ao trocar parâmetros para evitar
                // derivadas artificiais causadas pela mudança abrupta de Kd
                erro_anterior = 0;
            }
        }
        frame_counter++;

        cv::Mat frame;
        if (!cap.read(frame)) {
            std::cout << "Fim do video ou erro na leitura." << std::endl;
            break;
        }

        int width  = frame.cols;
        int height = frame.rows;

        // Define a Região de Interesse (ROI)
        cv::Rect roi_rect(0, height * (1 - altura_roi_percentual), width, height * altura_roi_percentual);
        cv::Mat roi = frame(roi_rect);

        // Processamento da imagem
        cv::Mat gray, blurred, thresh;
        cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, blurred, cv::Size(tamanho_kernel_blur, tamanho_kernel_blur), 0);
        cv::threshold(blurred, thresh, limiar_threshold, 255, cv::THRESH_BINARY);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

        int largestContourIndex = encontrarMaiorContorno(contours);
        
        int   erro         = 0;
        int   derivada     = 0;
        float ajuste       = 0;
        int   vel_esquerda = 0;
        int   vel_direita  = 0;
        std::string status = "Parado (Sem linha)";

        if (largestContourIndex != -1) {
            cv::Moments M = cv::moments(contours[largestContourIndex]);
            if (M.m00 != 0) {
                cv::Point2f center_roi(M.m10 / M.m00, M.m01 / M.m00);
                cv::Point2f center_frame = center_roi + cv::Point2f(roi_rect.x, roi_rect.y);
                
                // =========================================================
                // --- CÁLCULO DO CONTROLE PD ---
                // =========================================================

                int setPoint = width / 2;

                // 1. TERMO PROPORCIONAL
                erro = setPoint - (int)center_roi.x;

                // 2. TERMO DERIVATIVO
                derivada = erro - erro_anterior;

                // 3. AJUSTE TOTAL (usa cfg lido dinamicamente)
                ajuste = (cfg.kp * erro) + (cfg.kd * derivada);

                // 4. APLICA NAS VELOCIDADES
                vel_esquerda = cfg.velocidade_base - (int)ajuste;
                vel_direita  = cfg.velocidade_base + (int)ajuste;

                // Limita com vel_min e vel_max do config
                vel_esquerda = std::max(cfg.vel_min, std::min(cfg.vel_max, vel_esquerda));
                vel_direita  = std::max(cfg.vel_min, std::min(cfg.vel_max, vel_direita));

                // 5. ATUALIZA ERRO ANTERIOR
                erro_anterior = erro;

                // =========================================================

                status = "Esq: " + std::to_string(vel_esquerda) + " | Dir: " + std::to_string(vel_direita);
                
                // Desenhos na tela
                cv::drawContours(frame, contours, largestContourIndex, cv::Scalar(0, 255, 0), 2, cv::LINE_8, {}, 0, cv::Point(roi_rect.x, roi_rect.y));
                cv::circle(frame, center_frame, 7, cv::Scalar(0, 0, 255), -1);
                cv::line(frame, cv::Point(setPoint, 0), cv::Point(setPoint, height), cv::Scalar(255, 255, 0), 1);
            }
        } else {
            vel_esquerda  = 0;
            vel_direita   = 0;
            erro_anterior = 0;
        }
        
        // --- ENVIAR COMANDO SERIAL ---
        if (vel_esquerda != ultima_vel_esq || vel_direita != ultima_vel_dir) {
            enviarVelocidades(serial_port, vel_esquerda, vel_direita);
            
            if (largestContourIndex != -1) {
                std::cout << "[Kp:" << cfg.kp << " Kd:" << cfg.kd << "]"
                          << " Erro: "     << erro 
                          << " | Deriv: "  << derivada
                          << " | Ajuste: " << ajuste 
                          << " ==> Motores(Esq: " << vel_esquerda 
                          << ", Dir: "             << vel_direita << ")" << std::endl;
            } else {
                std::cout << "[Alerta] Linha perdida! ==> Parando motores (0, 0)" << std::endl;
            }

            ultima_vel_esq = vel_esquerda;
            ultima_vel_dir = vel_direita;
        }

        // Desenha a caixa da ROI e o texto de status
        // Exibe também os parâmetros atuais no canto superior direito
        cv::rectangle(frame, roi_rect, cv::Scalar(255, 0, 0), 2); 
        cv::putText(frame, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);

        std::string params_str = "Kp:" + std::to_string(cfg.kp).substr(0,4)
                               + " Kd:" + std::to_string(cfg.kd).substr(0,4);
        cv::putText(frame, params_str, cv::Point(width - 200, 30), cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(200, 200, 0), 1);

        cv::imshow("Visao do Robo (Colorida)", frame);
        cv::imshow("Mascara (Preto e Branco)", thresh);

        if (cv::waitKey(30) == 'q') {
            break;
        }
    }

    // --- LIMPEZA ---
    enviarVelocidades(serial_port, 0, 0); 
    cap.release();
    cv::destroyAllWindows();
    close(serial_port); 

    return 0;
}