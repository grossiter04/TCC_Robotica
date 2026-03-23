#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <fcntl.h>   
#include <termios.h> 
#include <unistd.h>  

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

    // --- PARÂMETROS DE VISÃO ---
    int limiar_threshold = 150;      
    int tamanho_kernel_blur = 7;     
    double altura_roi_percentual = 1; 

    // --- PARÂMETROS DO CONTROLE PD ---
    float Kp = 0.5;   // Ganho Proporcional: reage ao erro atual.
                      // Aumentar = reage mais forte, mas pode oscilar.
    
    float Kd = 0; // Ganho Derivativo: reage à VARIAÇÃO do erro.
                      // Aumentar = amortece mais as oscilações.
                      // Diminuir = menos amortecimento.
                      // DICA: comece com Kd entre 1x e 3x o valor de Kp.

    int velocidade_base = 130;   // Velocidade em linha reta (0-255)
    
    // --- VARIÁVEL DE MEMÓRIA DO ERRO ANTERIOR (essencial para o Kd) ---
    int erro_anterior = 0;

    int ultima_vel_esq =
     -1;
    int ultima_vel_dir = -1;

    std::cout << "Iniciando Controle PD. Pressione 'q' para sair." << std::endl;

    // --- CRIAÇÃO DAS JANELAS DE VISUALIZAÇÃO ---
    cv::namedWindow("Visao do Robo (Colorida)", cv::WINDOW_AUTOSIZE);
    cv::namedWindow("Mascara (Preto e Branco)", cv::WINDOW_AUTOSIZE);

    // --- LOOP PRINCIPAL ---
    while (true) {
        cv::Mat frame;
        if (!cap.read(frame)) {
            std::cout << "Fim do video ou erro na leitura." << std::endl;
            break;
        }

        int width = frame.cols;
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
        
        int erro = 0;
        float ajuste = 0;
        int vel_esquerda = 0;
        int vel_direita = 0;
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

                // 1. TERMO PROPORCIONAL (P):
                //    Mede o erro ATUAL (distância do centro até a linha).
                //    Quanto maior o erro, maior a correção.
                erro = setPoint - (int)center_roi.x;

                
                // 2. TERMO DERIVATIVO (D):
                //    Mede a VARIAÇÃO do erro entre o frame atual e o anterior.
                //    Se o erro está aumentando rápido (robô se afastando da linha),
                //    o Kd "freia" a correção, evitando oscilações e ultrapassagens.
                //    Se o erro está diminuindo (robô voltando para a linha),
                //    o Kd "afrouxa" a correção suavemente.
                int derivada = erro - erro_anterior;

                // 3. AJUSTE TOTAL (PD):
                //    A correção final é a soma dos dois termos.
                ajuste = (Kp * erro) + (Kd * derivada);

                // 4. APLICA O AJUSTE NAS VELOCIDADES DOS MOTORES:
                //    Motor esquerdo recebe MENOS velocidade quando o robô
                //    precisa virar à esquerda (erro positivo), e vice-versa.
                vel_esquerda = velocidade_base - (int)ajuste;
                vel_direita  = velocidade_base + (int)ajuste;
                
                // Limita as velocidades entre 100 e 255
                vel_esquerda = std::max(100, std::min(255, vel_esquerda));
                vel_direita  = std::max(100, std::min(255, vel_direita));

                // 5. ATUALIZA O ERRO ANTERIOR para o próximo frame
                erro_anterior = erro;

                // =========================================================

                status = "Esq: " + std::to_string(vel_esquerda) + " | Dir: " + std::to_string(vel_direita);
                
                // Desenhos na tela
                cv::drawContours(frame, contours, largestContourIndex, cv::Scalar(0, 255, 0), 2, cv::LINE_8, {}, 0, cv::Point(roi_rect.x, roi_rect.y));
                cv::circle(frame, center_frame, 7, cv::Scalar(0, 0, 255), -1);
                cv::line(frame, cv::Point(setPoint, 0), cv::Point(setPoint, height), cv::Scalar(255, 255, 0), 1);
            }
        } else {
            // Linha perdida: para os motores e ZERA o erro anterior
            // para evitar uma derivada falsa no próximo frame que
            // detectar a linha.
            vel_esquerda = 0;
            vel_direita = 0;
            erro_anterior = 0;
        }
        
        // --- ENVIAR COMANDO SERIAL ---
        if (vel_esquerda != ultima_vel_esq || vel_direita != ultima_vel_dir) {
            enviarVelocidades(serial_port, vel_esquerda, vel_direita);
            
            if (largestContourIndex != -1) {
                int derivada_print = erro - erro_anterior; // Para exibição
                std::cout << "[Kp:" << Kp << " Kd:" << Kd << "]"
                          << " Erro: " << erro 
                          << " | Derivada: " << (erro - erro_anterior)
                          << " | Ajuste: " << ajuste 
                          << " ==> Motores(Esq: " << vel_esquerda 
                          << ", Dir: " << vel_direita << ")" << std::endl;
            } else {
                std::cout << "[Alerta] Linha perdida! ==> Parando motores (0, 0)" << std::endl;
            }

            ultima_vel_esq = vel_esquerda;
            ultima_vel_dir = vel_direita;
        }

        // Desenha a caixa da ROI e o texto de status
        cv::rectangle(frame, roi_rect, cv::Scalar(255, 0, 0), 2); 
        cv::putText(frame, status, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);

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