/*
===============================================================================
    Projeto: BuzzLine - Versão Controle PID (Apenas P implementado)
    
    - Recebe uma string "VelEsquerda,VelDireita\n" da Raspberry Pi.
    - Aplica o PWM (0 a 255) nos motores em tempo real.
===============================================================================
*/

// --- Pinos da Ponte H L9110 ---
const int pinoMotorEsquerdoA = 5;  // (L9110 A-1A) - PWM
const int pinoMotorEsquerdoB = 6;  // (L9110 A-1B) - Direção
const int pinoMotorDireitoA = 9;   // (L9110 B-1A) - PWM
const int pinoMotorDireitoB = 10;  // (L9110 B-1B) - Direção

void setup() {
  pinMode(pinoMotorEsquerdoA, OUTPUT);
  pinMode(pinoMotorEsquerdoB, OUTPUT);
  pinMode(pinoMotorDireitoA, OUTPUT);
  pinMode(pinoMotorDireitoB, OUTPUT);
  
  Serial.begin(9600);
  
  // CRÍTICO: Reduz o tempo de espera pela leitura da string para 10 milissegundos.
  // Sem isso, a leitura serial gera um lag de 1 segundo e o robô fica louco.
  Serial.setTimeout(10); 

  parar();
}

void loop() {
  // Verifica se há dados da Raspberry Pi disponíveis
  if (Serial.available() > 0) {
    // Lê toda a string até achar a quebra de linha '\n' enviada pela Pi
    String comando = Serial.readStringUntil('\n');
    
    // Procura onde está a vírgula
    int indexVirgula = comando.indexOf(',');
    
    // Se encontrou a vírgula, a formatação está correta
    if (indexVirgula > 0) {
      // Separa a string em duas partes e converte para número inteiro (int)
      int velEsquerda = comando.substring(0, indexVirgula).toInt();
      int velDireita = comando.substring(indexVirgula + 1).toInt();
      
      // Aplica a velocidade nos motores
      moverMotores(velEsquerda, velDireita);
    }
  }
}

// --- FUNÇÃO DE CONTROLE DOS MOTORES ---

void moverMotores(int velEsq, int velDir) {
  // Configura Motor Esquerdo (Sempre indo para frente com velocidade variável)
  analogWrite(pinoMotorEsquerdoA, velEsq);
  digitalWrite(pinoMotorEsquerdoB, LOW);
  
  // Configura Motor Direito (Sempre indo para frente com velocidade variável)
  analogWrite(pinoMotorDireitoA, velDir);
  digitalWrite(pinoMotorDireitoB, LOW);
}

void parar() {
  digitalWrite(pinoMotorEsquerdoA, LOW);
  digitalWrite(pinoMotorEsquerdoB, LOW);
  digitalWrite(pinoMotorDireitoA, LOW);
  digitalWrite(pinoMotorDireitoB, LOW);
}