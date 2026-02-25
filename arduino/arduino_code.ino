/*
===============================================================================
    Projeto: BuzzLine - Versão Visão Computacional (Modificada)
    
    - Recebe comandos ('F', 'L', 'R', 'P') da Raspberry Pi.
    - Controla a Ponte H L9110 com os pinos D5, D6, D9, D10.
    - Lógica de curva alterada para "skid steer" (roda interna para).
    - Velocidades de reta e curva ajustáveis.
===============================================================================
*/

// --- Pinos da Ponte H L9110 ---
const int pinoMotorEsquerdoA = 5;  // (L9110 A-1A)
const int pinoMotorEsquerdoB = 6;  // (L9110 A-1B)
const int pinoMotorDireitoA = 9;   // (L9110 B-1A)
const int pinoMotorDireitoB = 10;  // (L9110 B-1B)

// --- Velocidades dos Motores (0 = parado, 255 = máximo) ---
// --- AJUSTE ESTES VALORES ---
const int velocidadeFrente = 170; // Velocidade para ir reto. Comece com um valor mais baixo (ex: 130)
const int velocidadeCurva = 130;  // Velocidade da roda externa ao curvar (pode ser maior para curvas mais rápidas)
const int velocidadeInverso = 10;

void setup() {
  /*Configuracao dos pinos de entrada e saida */
  pinMode(pinoMotorEsquerdoA, OUTPUT);
  pinMode(pinoMotorEsquerdoB, OUTPUT);
  pinMode(pinoMotorDireitoA, OUTPUT);
  pinMode(pinoMotorDireitoB, OUTPUT);
  
  /*Configuracao da porta serial para receber comandos da Pi*/
  Serial.begin(9600);

  // Garante que o robô comece parado
  parar();
}

void loop() {
  // Verifica se há algum comando vindo da Pi
  if (Serial.available() > 0) {
    // Lê o caractere do comando
    char comando = Serial.read();

    // Executa a ação baseada no comando
    switch (comando) {
      case 'F': // Frente
        irFrente();
        break;
      case 'L': // Esquerda (da Pi)
        virarEsquerda();
        break;
      case 'R': // Direita (da Pi)
        virarDireita();
        break;
      case 'P': // Parar
        parar();
        break;
    }
  }
}

// --- FUNÇÕES DE MOVIMENTO PARA A L9110 ---

void irFrente() {
  // Motor Esquerdo: para frente
  analogWrite(pinoMotorEsquerdoA, velocidadeFrente);
  digitalWrite(pinoMotorEsquerdoB, LOW);
  
  // Motor Direito: para frente
  analogWrite(pinoMotorDireitoA, velocidadeFrente);
  digitalWrite(pinoMotorDireitoB, LOW);
}

void virarEsquerda() {
  // Roda esquerda (interna) PARA
  digitalWrite(pinoMotorEsquerdoA, LOW);
  digitalWrite(pinoMotorEsquerdoB, LOW);
  
  // Roda direita (externa) para FRENTE
  analogWrite(pinoMotorDireitoA, velocidadeCurva);
  digitalWrite(pinoMotorDireitoB, LOW);
}

void virarDireita() {
  // Roda esquerda (externa) para FRENTE
  analogWrite(pinoMotorEsquerdoA, velocidadeCurva);
  digitalWrite(pinoMotorEsquerdoB, LOW);
  
  // Roda direita (interna) PARA
  digitalWrite(pinoMotorDireitoA, LOW);
  digitalWrite(pinoMotorDireitoB, LOW);
}

void parar() {
  // Desliga todos os pinos (freio)
  digitalWrite(pinoMotorEsquerdoA, LOW);
  digitalWrite(pinoMotorEsquerdoB, LOW);
  digitalWrite(pinoMotorDireitoA, LOW);
  digitalWrite(pinoMotorDireitoB, LOW);
}
