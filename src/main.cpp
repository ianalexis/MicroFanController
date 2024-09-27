#include <Arduino.h>

// Pines de conexion
static const int pinPWM = D1;
static const int pinTacometro = D2;
static const int pinTermistor = A0;

// Variables globales
static const int temperaturaMaximaEmergencia = 90; // Temperatura maxima de emergencia.
static const int tiempoDeMuestreo = 5000; // Tiempo de espera en milisegundos entre lecturas de temperatura.
int tempMin = 20; // Temperatura minima en la curva segun velocidad minima(se setea en setTempMin()). //TODO: Subir a 40 grados.
bool tacometro = true; // Variable para saber si el tacometro esta funcionando.
const int erroresVelocidadMax = 5; // Cantidad maxima de errores de velocidad.
int erroresVelocidad = 0; // Cantidad de errores de velocidad.
int temperatura = 0; // Temperatura actual.

// Variables de control de velocidad
static const int pwmMax = 100; // Valor maximo de PWM
static const int pwmMinSinTacometro = 35; // Porcentaje de la velocidad minima para el caso de no tener sensor.
static const int pwmOff = 1; // Valor de PWM para apagar el motor.
int pwmMin = 10; // Valor minimo de PWM (se setea en setPWMMin()).
int pwm = 0; // Valor de PWM actual.


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

// Matriz de temperatura y porcentaje de PWM (constante) //TODO: Despues de la prueba, descomentar la tabla original y ajustarla para usar min y max.
// const TempPWM tempPWMArray[] = {
//  {35, 1},
//  {40, 35},
//  {65, 50},
//  {75, 65},
//  {80, 90},
//  {90, 100}
//};

 const TempPWM tempPWMArray[] = {
  {20, 30},
  {25, 35},
  {60, 50},
  {70, 65},
  {80, 90},
  {90, 100}
}; // Tabla de prueba TODO: Borrar


// Número de elementos en la matriz
const int cantElementosArray = sizeof(tempPWMArray) / sizeof(tempPWMArray[0]);

// Prototipos de función
void setup();
void loop();
void setmins();
void setPWMMin();
void setTempMin();
int pwmDeArranque();
void setVelocidadPWM(int velocidad);
bool enMovimiento();
int temperaturaTermistor();
int calcularPWM(int temperatura);
int velocidadActual();
void verificarTemperatura(int temperatura);
void verificarVelocidad();

void setup() {
  Serial.println("Iniciando sistema...");
  Serial.begin(9600);
  Serial.println("Configurando pines...");
  pinMode(LED_BUILTIN, OUTPUT); // Setea el pin del LED incorporado como salida.
  pinMode(pinPWM, OUTPUT); // Setea el pin 9 como salida.
  pinMode(pinTacometro, INPUT); // Setea el pin como entrada.
  analogWriteFreq(25000); // Setea la frecuencia de PWM a 25kHz.
  setmins(); // Setea el valor de PWM minimo y la temperatura minima.
  Serial.println("Sistema iniciado correctamente.");
  }

void setmins() {
  Serial.println("Seteando valores minimos...");
  setPWMMin();
  setTempMin();
}

// Setea el valor de pwmMin con el valor de la velocidad minima del sensor y en caso de no existir o no funcionar el sensor, se setea en un 75% de la velocidad PWM.
void setPWMMin() {
  Serial.println("Seteando PWM minimo...");
  pwmMin = pwmDeArranque();
  tacometro = pwmMin > 0; // Si el PWM minimo es mayor a 0, el tacometro esta funcionando.
  if (!tacometro) {
    Serial.println("Error: No se detecto movimiento en el tacometro, se setea el PWM minimo en un " + String(pwmMinSinTacometro) + "%");
    pwmMin = pwmMinSinTacometro;
  }
}

// Setea el valor de tempMin con el valor de PWM minimo en base a la tabla de temperaturas y PWM para evitar enviar un PWM menor al minimo.
void setTempMin() {
  Serial.println("Seteando temperatura minima...");
  tempMin = tempPWMArray[0].temperatura; // Setea el valor de tempMin con el valor de la primera temperatura de la tabla.
  for (int i = 0; i < cantElementosArray; i++) { // Recorre la tabla de temperaturas y PWM.
    if (tempPWMArray[i].porcentajePWM >= pwmMin) { // Si el porcentaje de PWM es mayor o igual al porcentaje minimo.
      Serial.println("Temperatura minima: " + String(tempPWMArray[i].temperatura) + "°C");
      tempMin = tempPWMArray[i].temperatura; // Setea el valor de tempMin con la temperatura de la tabla.
      return;
    }
  }
  tempMin = 10;
  Serial.println("Error en Tabla: Temperatura minima no encontrada en la tabla, se setea en " + String(tempMin) + "°C");
}

// Devuelve la velocidad minima en PWM recorriendo el rango PWM hasta detectar movimiento. Devuelve un 10% mas de la velocidad detectada y si no detecta movimiento, devuelve 0.
int pwmDeArranque() {
  for (int i = pwmMin; i < pwmMinSinTacometro; i++) { // Recorre el rango de PWM de 0 a el maximo.
    setVelocidadPWM(i); // Setea el valor de PWM.
    if (enMovimiento()) {
      Serial.println("Velocidad minima detectada: " + String(i + 2) + "%");
      return static_cast < int > (i + 2);
    }
  }
  Serial.println("Error: No se detecto movimiento en el tacometro.");
  return 0;
}

//------------------------------------------------------------------------------------------------------------------------------------------------

void loop() {
  temperatura = temperaturaTermistor(); // Lee la temperatura del termistor.
  verificarTemperatura(temperatura); // Verifica si la temperatura supera la temperatura de emergencia.
  pwm = calcularPWM(temperatura); // Calcula el valor de PWM basado en la temperatura
  setVelocidadPWM(pwm); // Setea el valor de PWM.
  delay(tiempoDeMuestreo); // Espera para volver a leer la temperatura.
  verificarVelocidad(); // Verifica si el motor esta en movimiento si el tacometro esta activo.
}

// Calcula el valor de PWM basado en la temperatura utilizando interpolación lineal
int calcularPWM(int temperatura) {
  if (temperatura < tempMin) { // Si la temperatura es menor a la temperatura minima, devuelve el PWM minimo.
    Serial.println("Temperatura menor a la minima, apagando motor.");
    return pwmOff; // Apaga el motor si la temperatura es menor a la minima.
  }
  if (temperatura >= tempPWMArray[cantElementosArray - 1].temperatura) {
    Serial.println("Temperatura mayor a la maxima. Encendiendo motor al maximo.");
    return pwmMax; // Enciende el motor al maximo si la temperatura es mayor a la maxima.
  }
  for (int i = 0; i < cantElementosArray - 1; i++) {
    if (temperatura >= tempPWMArray[i].temperatura && temperatura <= tempPWMArray[i + 1].temperatura) {
      int tempDiff = tempPWMArray[i + 1].temperatura - tempPWMArray[i].temperatura;
      int pwmDiff = tempPWMArray[i + 1].porcentajePWM - tempPWMArray[i].porcentajePWM;
      int tempOffset = temperatura - tempPWMArray[i].temperatura;
      int porcentajePWM = tempPWMArray[i].porcentajePWM + (pwmDiff * tempOffset) / tempDiff;
      Serial.println("Porcentaje de PWM: " + String(porcentajePWM) + "%");
      return porcentajePWM;
    }
  }
  Serial.println("Error: No se encontro el valor de PWM para la temperatura.");
  return 0; // Valor por defecto en caso de error
}

// Devuelve si detecta movimiento en el tacometro.
bool enMovimiento() {
  Serial.println("Detectando movimiento...");
  return velocidadActual() > 0;
}

// Verifica si la temperatura supera la temperatura de emergencia.
void verificarTemperatura(int temperatura) {
  if (temperatura >= temperaturaMaximaEmergencia) {
    setVelocidadPWM(pwmMax); // Setea el valor de PWM.
    digitalWrite(LED_BUILTIN, HIGH); // Enciende el LED incorporado.
    Serial.println("Advertencia: Temperatura de emergencia detectada. Encendiendo motor al maximo.");
    delay(10000); // Delay de 10 segundos a maxima velocidad antes de la proxima lectura.
    digitalWrite(LED_BUILTIN, LOW); // Apaga el LED incorporado.
  }
}

// Devuelve la velocidad actual del motor en RPM.
int velocidadActual() {
  unsigned long tiempo = 1000; // Tiempo de espera en milisegundos para calcular la velocidad.
  int pulsos = 0; // Cantidad de pulsos detectados.
  unsigned long tiempoInicial = millis(); // Tiempo inicial.
  while (millis() - tiempoInicial < tiempo) { // Mientras no se cumpla el tiempo de espera.
    if (digitalRead(pinTacometro) == HIGH) { // Si detecta un pulso.
      pulsos++; // Incrementa la cantidad de pulsos.
      while (digitalRead(pinTacometro) == HIGH) {} // Espera a que el pulso termine.
    }
    delay(1); // Agrega un pequeño retraso para evitar el WDT reset.
  }  
  int velocidad = pulsos * 60 / 2; // Calcula la velocidad en RPM.
  Serial.println("Velocidad: " + String(velocidad) + " RPM");
  return velocidad;
}

// Devuelve la temperatura del termistor.
int temperaturaTermistor() {
  float lectura = analogRead(pinTermistor); // Lectura del termistor.
  if (lectura < 0 || lectura > 1023) { // Si la lectura esta fuera de rango, devuelve 0.
    Serial.println("Error: Lectura del termistor fuera de rango");
    return 0; // Valor por defecto en caso de error.
  }
  float R = tR1 * (1023.0 / lectura - 1.0); // Resistencia del termistor.
  //Serial.println("Resistencia: " + String(R) + " ohms");
  float T = 1.0 / (1.0 / tT0 + log(R / tR0) / tBeta); // Temperatura en Kelvin.
  Serial.println("Temperatura: " + String(T - 273.15) + "°C");
  return T - 273.15; // Temperatura en Celsius.
}

// Setea la velocidad del motor en PWM.
void setVelocidadPWM(int velocidad) { // TODO: REVISAR si es suficiente o se necesita otros cambios de frecuencia.
  Serial.println("Velocidad: " + String(velocidad) + "%");
  analogWrite(pinPWM, map(velocidad, 0, 100, 0, 255));
}

void verificarVelocidad() {
  if (tacometro && pwm > pwmMin && !enMovimiento()) {
    erroresVelocidad++;
    while (!enMovimiento() && erroresVelocidad < erroresVelocidadMax) {
      setVelocidadPWM(pwmMax);
      delay(1000);
      if (enMovimiento()){
        Serial.println("Motor encendido al 100%, recalibrando velocidad minima.");
        pwmMin = pwmDeArranque();
        setTempMin();
        erroresVelocidad--;
      }
      erroresVelocidad++;
    }
    if (erroresVelocidad >= erroresVelocidadMax) {
      Serial.println("Error: No se detecto movimiento en el tacometro.");
      Serial.println("Apagando motor y reiniciando sistema.");
      setVelocidadPWM(pwmOff);
      delay(1000);
      setVelocidadPWM(0);
      delay(1000);
      ESP.restart(); // Reinicia el sistema
    }
  }
}