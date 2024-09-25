#include <Arduino.h>
 // Pines de conexion
static const int pinPWM = 9;
static const int pinSensor = 2;
static const int pinTermistor = A0;

// Variables globales
static const int pwmMax = 100; // Valor maximo de PWM
static const int sensorlessMinPorcentaje = 35; // Porcentaje de la velocidad minima para el caso de no tener sensor.
static const int temperaturaMaximaEmergencia = 90; // Temperatura maxima de emergencia.
static const int tiempoDeMuestreo = 500; // Tiempo de espera en milisegundos entre lecturas de temperatura.
static const int pwmOff = 1; // Valor de PWM para apagar el motor.
int pwmMin = 0; // Valor minimo de PWM (inicializado en 0) (se setea en setPWMMin()).
int tempMin = 0; // Temperatura minima (inicializado en 0) (se setea en setTempMin()).

// Variables de Termistor
const float tBeta = 4021; // Valor B del termistor. Intermedio entre NTC3950 100K y EPKOS 4092 100K.
const float tR0 = 100000; // Valor de resistencia a 25 grados Celsius.
const float tT0 = 298.15; // Temperatura de referencia en Kelvin.
const float tR1 = 10000; // Resistencia en serie (10K ohms). De referencia para el divisor de voltaje.

// Estructura para almacenar pares de temperatura y porcentaje de PWM
struct TempPWM {
  int temperatura;
  int porcentajePWM;
};

// Matriz de temperatura y porcentaje de PWM (constante)
const TempPWM tempPWMArray[] = {
    {40, 1},
    {65, 25},
    {77, 50},
    {85, 75},
    {90, 100}
};

// Número de elementos en la matriz
const int cantElementosArray = sizeof(tempPWMArray) / sizeof(tempPWMArray[0]);

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT); // Setea el pin del LED incorporado como salida.
  digitalWrite(LED_BUILTIN, HIGH); // Enciende el LED incorporado.
  pinMode(pinPWM, OUTPUT); // Setea el pin 9 como salida.
  pinMode(pinSensor, INPUT); // Setea el pin 2 como entrada.
  setmins(); // Setea el valor de PWM minimo y la temperatura minima.
  digitalWrite(LED_BUILTIN, LOW); // Apaga el LED incorporado.
}

void setmins() {
  setPWMMin();
  setTempMin();
}

// Setea el valor de pwmMin con el valor de la velocidad minima del sensor y en caso de no existir o no funcionar el sensor, se setea en un 75% de la velocidad PWM.
void setPWMMin() {
  pwmMin = pwmDeArranque();
  if (pwmMin == 0) {
    pwmMin = sensorlessMinPorcentaje;
  }
}

// Setea el valor de tempMin con el valor de PWM minimo en base a la tabla de temperaturas y PWM para evitar enviar un PWM menor al minimo.
void setTempMin() {
  tempMin = tempPWMArray[0].temperatura; // Setea el valor de tempMin con el valor de la primera temperatura de la tabla.
  for (int i = 0; i < cantElementosArray; i++) { // Recorre la tabla de temperaturas y PWM.
    if (tempPWMArray[i].porcentajePWM >= (pwmMin * 100) / pwmMax) { // Si el porcentaje de PWM es mayor o igual al porcentaje minimo.
      tempMin = tempPWMArray[i].temperatura; // Setea el valor de tempMin con la temperatura de la tabla.
      break;
    }
  }
}

// Devuelve la velocidad minima en PWM recorriendo el rango PWM hasta detectar movimiento. Devuelve un 10% mas de la velocidad detectada y si no detecta movimiento, devuelve 0.
int pwmDeArranque() {
  for (int i = 10; i < sensorlessMinPorcentaje; i++) { // Recorre el rango de PWM de 0 a el maximo.
    setVelocidadPWM(i); // Setea el valor de PWM.
    if (sensorEnMovimiento()) {
      return static_cast < int > (i + 2);
    }
  }
  return 0;
}

//--------------------------------------------------------------

void loop() {
  int temperatura = temperaturaTermistor(); // Lee la temperatura del termistor.
  if (temperatura >= temperaturaMaximaEmergencia) { // Si la temperatura supera la temperatura de emergencia, enciende el motor al maximo y prende un led de advertencia.
    setVelocidadPWM(pwmMax); // Setea el valor de PWM.
    digitalWrite(LED_BUILTIN, HIGH); // Enciende el LED incorporado.
    delay(30000); // Espera 30 segundos para intentar bajar la temperatura.
    digitalWrite(LED_BUILTIN, LOW); // Apaga el LED incorporado.
  } else {
    int pwm = calcularPWM(temperatura); // Calcula el valor de PWM basado en la temperatura
    setVelocidadPWM(pwm); // Setea el valor de PWM.
    delay(tiempoDeMuestreo); // Espera para volver a leer la temperatura.
  }
}

// Calcula el valor de PWM basado en la temperatura utilizando interpolación lineal
int calcularPWM(int temperatura) {
  if (temperatura < tempMin) { // Si la temperatura es menor a la temperatura minima, devuelve el PWM minimo.
    return pwmOff; // Apaga el motor si la temperatura es menor a la minima.
  }
  if (temperatura >= tempPWMArray[cantElementosArray - 1].temperatura) {
    return pwmMax; // Enciende el motor al maximo si la temperatura es mayor a la maxima.
  }
  for (int i = 0; i < cantElementosArray - 1; i++) {
    if (temperatura >= tempPWMArray[i].temperatura && temperatura <= tempPWMArray[i + 1].temperatura) {
      int tempDiff = tempPWMArray[i + 1].temperatura - tempPWMArray[i].temperatura;
      int pwmDiff = tempPWMArray[i + 1].porcentajePWM - tempPWMArray[i].porcentajePWM;
      int tempOffset = temperatura - tempPWMArray[i].temperatura;
      int porcentajePWM = tempPWMArray[i].porcentajePWM + (pwmDiff * tempOffset) / tempDiff;
      return porcentajePWM;
    }
  }
  return 0; // Valor por defecto en caso de error
}

// Devuelve si el sensor detecto 20 pasadas por 1 segundo.
bool sensorEnMovimiento() {
  int contadorSeñales = 0;
  unsigned long tiempoInicio = millis();
  while (millis() - tiempoInicio < 500) { // 1000 ms = 1 segundo
    if (digitalRead(pinSensor) == HIGH) {
      contadorSeñales++;
      // Esperar a que la señal baje para evitar contar múltiples veces la misma señal
      while (digitalRead(pinSensor) == HIGH);
    }
  }
  return contadorSeñales >= 20;
}

// Devuelve la temperatura del termistor.
int temperaturaTermistor() {
  float lectura = analogRead(pinTermistor); // Lectura del termistor.
  float R = tR1 * (1023.0 / lectura - 1.0); // Resistencia del termistor.
  float T = 1.0 / (1.0 / tT0 + log(R / tR0) / tBeta); // Temperatura en Kelvin.
  return T - 273.15; // Temperatura en Celsius.
}

// Setea la velocidad del motor en PWM.
void setVelocidadPWM(int velocidad) {
  analogWrite(9, map(velocidad, 0, 100, 0, 255));
}