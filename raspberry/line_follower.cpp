#include <iostream>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
// --- INCLUSÕES PARA A PORTA SERIAL ---
#include <fcntl.h>   // Para controle de arquivos (serial port)
#include <termios.h> // Para configuração da serial
#include <unistd.h>  // Para read/write/close

// --- FUNÇÕES AUXILIARES PARA A SERIAL ---

// Função para configurar e abrir a porta serial no Linux
int abrirPortaSerial(const char* portname) {
    // Abre a porta serial. O_RDWR = Leitura/Escrita. O_NOCTTY = Não se torna o terminal de controle.
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

    // Define a velocidade da porta (Baud Rate) para 9600
    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    // Configurações da porta: 8 bits de dados, sem paridade, 1 stop bit
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

    // Aplica as configurações
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Erro ao setar atributos da serial" << std::endl;
        return -1;
    }
    return fd;
}

// Função para enviar um comando (um único caractere)
void enviarComando(int serial_port, char comando) {
    write(serial_port, &comando, 1); // Escreve 1 byte (o caractere) na porta
}

// --- FIM DAS FUNÇÕES DA SERIAL ---

// Função para encontrar o maior contorno (não muda)
int encontrarMaiorContorno(const std::vector<std::vector<cv::Point>>& contours) {
    if (contours.empty()) return -1;
    double maxArea = 0.0;
    int largestContourIndex = -1;
    for (int i = 0; i < contours.size(); i++) {
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
    // ATUALIZADO com a porta que você descobriu!
    const char* porta_serial_nome = "/dev/ttyUSB0"; 
    
    int serial_port = abrirPortaSerial(porta_serial_nome);
    if (serial_port < 0) {
        std::cerr << "Nao foi possivel conectar ao Arduino. Verifique a porta." << std::endl;
        std::cerr << "Dica: Voce rodou 'sudo usermod -a -G dialout seu_usuario'?" << std::endl;
        return -1; 
    }
    std::cout << "Conectado ao Arduino na porta " << porta_serial_nome << std::endl;
    // ------------------------------------

    // --- CONFIGURAÇÃO DA CÂMERA DA PI ---
    std::string pipeline = "libcamerasrc ! video/x-raw,width=640,height=480,framerate=30/1 ! videoconvert ! appsink";
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);

    if (!cap.isOpened()) {
        std::cerr << "Erro: Nao foi possivel abrir a camera." << std::endl;
        return -1;
    }

    // --- PARÂMETROS DE VISÃO ---
    int limiar_threshold = 150;      
    int tamanho_kernel_blur = 7;     
    double area_minima_contorno = 1000.0; 
    double altura_roi_percentual = 0.4; 

    //Caso queira ver a janela da camera, tirar o comentario das duas linhas de cv::nameWindow

    // Cria as janelas (VNC irá mostrá-las se você estiver conectado)
    //cv::namedWindow("Video da Pista", cv::WINDOW_NORMAL);
    //cv::namedWindow("Mascara Branca (Threshold)", cv::WINDOW_NORMAL);

    std::cout << "Pressione 'q' para sair." << std::endl;
    std::string lastDirection = "";

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

        // Processamento da imagem (só na ROI)
        cv::Mat gray, blurred, thresh;
        cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, blurred, cv::Size(tamanho_kernel_blur, tamanho_kernel_blur), 0);
        cv::threshold(blurred, thresh, limiar_threshold, 255, cv::THRESH_BINARY);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

        std::string direction = "Procurando linha...";
        char comando_serial = 'P'; // Comando 'P' para Parar (default)

        int largestContourIndex = encontrarMaiorContorno(contours);
        
        if (largestContourIndex != -1) {
            cv::Moments M = cv::moments(contours[largestContourIndex]);
            if (M.m00 != 0) {
                cv::Point2f center_roi(M.m10 / M.m00, M.m01 / M.m00);
                cv::Point2f center_frame = center_roi + cv::Point2f(roi_rect.x, roi_rect.y);
                int limite_esquerda = width / 3;
                int limite_direita = 2 * (width / 3);
                
                // --- MAPEAMENTO DE DIREÇÃO PARA COMANDO SERIAL ---
                if (center_roi.x < limite_esquerda) {
                    direction = "Virar a Esquerda";
                    comando_serial = 'L'; // 'L' para Esquerda
                } else if (center_roi.x > limite_direita) {
                    direction = "Virar a Direita";
                    comando_serial = 'R'; // 'R' para Direita
                } else {
                    direction = "Em Frente";
                    comando_serial = 'F'; // 'F' para Frente
                }
                
                // Desenha na imagem original (frame)
                cv::drawContours(frame, contours, largestContourIndex, cv::Scalar(0, 255, 0), 2, cv::LINE_8, {}, 0, cv::Point(roi_rect.x, roi_rect.y));
                cv::circle(frame, center_frame, 7, cv::Scalar(0, 0, 255), -1);
            }
        }
        
        // Desenha a caixa da ROI e o texto
        cv::rectangle(frame, roi_rect, cv::Scalar(255, 0, 0), 2); 
        cv::putText(frame, direction, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);

        // --- ENVIAR COMANDO SERIAL APENAS QUANDO A DIREÇÃO MUDAR ---
        if (direction != lastDirection) {
            std::cout << "Nova Direcao: " << direction << " (Enviando: " << comando_serial << ")" << std::endl;
            enviarComando(serial_port, comando_serial); // Envia o comando para o Arduino
            lastDirection = direction;
        }

        //Se quiser ver a janela da camera, tirar os comentarios de cv::imShow
        
        // Mostra as imagens (visível se você usar um VNC)
        //cv::imshow("Video da Pista", frame);
        //cv::imshow("Mascara Branca (Threshold)", thresh);

        if (cv::waitKey(30) == 'q') {
            break;
        }
    }

    // --- LIMPEZA ---
    cap.release();

    //Se quiser ver a janela da camera, tirar o comentario de cv::destroyAllWindows
    
    //cv::destroyAllWindows();
    close(serial_port); // Fecha a porta serial

    return 0;
}
