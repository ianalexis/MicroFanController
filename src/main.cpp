#include <Arduino.h>
// TODO: REVISAR QUE VARIABLES SE PUEDEN PASAR A UNASIGNED. SOLO EN CUENTAS QUE NUNCA PUEDA HABER UN NEGATIVO O UN INT PORQUE ES PEOR USAR UN UNSIGNED CON UN INT.
// Pines de conexion
static const int pinPWM = D1;		// Pin de control de velocidad del motor.
static const int pinTacometro = D2; // Pin de lecturaTermistor del tacometro.
static const int pinTermistor = A0; // Pin de lecturaTermistor del termistor.

// Variables globales
static const int temperaturaMaximaEmergencia = 90; // Temperatura maxima de emergencia.
static const int tiempoDeMuestreo = 5000;		   // Tiempo de espera en milisegundos entre lecturaTermistors de temperatura.
int tempMin = 20;								   // Temperatura minima en la curva segun velocidad minima(se setea en setTempMin()). //TODO: Subir a 40 grados.
bool tacometro = true;							   // Variable para saber si el tacometro esta funcionando.
const int erroresVelocidadMax = 5;				   // Cantidad maxima de errores de velocidad.
int erroresVelocidad = 0;						   // Cantidad de errores de velocidad.
int temperatura = 0;							   // Temperatura actual.

// Variables de control de velocidad
static const int pwmMax = 100;			  // Valor maximo de PWM
static const int pwmMinSinTacometro = 40; // Porcentaje de la velocidad minima para el caso de no tener sensor.
static const int pwmOff = 1;			  // Valor de PWM para apagar el motor.
static const int pwmFreq = 25000;		  // Frecuencia de PWM en Hz. 25kHz segun Noctua WhitePaper.
int pwmMin = 10;						  // Valor minimo de PWM (se setea en setPWMMin()).
int pwm = 0;							  // Valor de PWM actual.
// TODO: Valor de diferencia +/- para cambio de velocidad.

// Variables de Termistor
const int tBeta = 4021; // Valor B del termistor. Intermedio entre NTC3950 100K y EPKOS 4092 100K.
const int tR0 = 100000; // Valor de resistencia a 25 grados Celsius.
const int tT0 = 298.15; // Temperatura de referencia en Kelvin.
const int tR1 = 10000;  // Resistencia en serie (10K ohms). De referencia para el divisor de voltaje.
const int kelvin = 273.15; // Valor de Kelvin para convertir a Celsius.
int lecturaTermistor = 0; // Lectura del termistor.

//Variables Tacometro
int velocidad = 0; // Velocidad actual del motor en RPM.
int pulsos = 0; // Cantidad de pulsos detectados.
const int tiempoMaxEspera = 200;    // Tiempo máximo de espera en milisegundos para calcular la velocidad.
const int tiempoMaxEsperaPulso = 100; // Tiempo máximo de espera para cada pulso en milisegundos.
int tiempoInicial = 0; // Tiempo inicial de la medición de pulsos.
int tiempoMedido = 0; // Tiempo de medición de pulsos en milisegundos.



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
	{90, 100}}; // Tabla de prueba TODO: Borrar

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

// Inicializa el sistema
void setup() {
	Serial.println("Iniciando sistema...");
	Serial.begin(9600);
	Serial.println("Configurando pines...");
	pinMode(LED_BUILTIN, OUTPUT); // Setea el pin del LED incorporado como salida.
	pinMode(pinPWM, OUTPUT);	  // Setea el pin 9 como salida.
	pinMode(pinTacometro, INPUT); // Setea el pin como entrada.
	analogWriteFreq(pwmFreq);	  // Setea la frecuencia de PWM.
	setmins();					  // Setea el valor de PWM minimo y la temperatura minima.
	Serial.println("Sistema iniciado correctamente.");
}

// Setea los valores minimos de PWM y temperatura.
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
		Serial.print("Error: No se detecto movimiento en el tacometro, se setea el PWM minimo en un ");
		Serial.print(pwmMinSinTacometro);
		Serial.println("%");
		pwmMin = pwmMinSinTacometro;
	}
}

// Setea el valor de tempMin con el valor de PWM minimo en base a la tabla de temperaturas y PWM para evitar enviar un PWM menor al minimo.
void setTempMin() {
	Serial.println("Seteando temperatura minima...");
	tempMin = tempPWMArray[0].temperatura; // Setea el valor de tempMin con el valor de la primera temperatura de la tabla.
	for (int i = 0; i < cantElementosArray; i++) { // Recorre la tabla de temperaturas y PWM.
		if (tempPWMArray[i].porcentajePWM >= pwmMin) {// Si el porcentaje de PWM es mayor o igual al porcentaje minimo.
			Serial.print("Temperatura minima: ");
			Serial.print(tempPWMArray[i].temperatura);
			Serial.println("°C");
			tempMin = tempPWMArray[i].temperatura; // Setea el valor de tempMin con la temperatura de la tabla.
			return;
		}
	}
	tempMin = 10;
	Serial.print("Error en Tabla: Temperatura minima no encontrada en la tabla, se setea en ");
	Serial.print(tempMin);
	Serial.println("°C");
}

// Devuelve la velocidad minima en PWM recorriendo el rango PWM hasta detectar movimiento. Devuelve un 10% mas de la velocidad detectada y si no detecta movimiento, devuelve 0.
int pwmDeArranque() {
	for (int i = pwmMin; i < pwmMinSinTacometro; i++) {						// Recorre el rango de PWM de 0 a el maximo.
		setVelocidadPWM(i); // Setea el valor de PWM.
		if (enMovimiento()) {
			Serial.print("Velocidad minima detectada: ");
			Serial.print(i + 2);
			Serial.println("%");
			return static_cast<int>(i + 2);
		}
	}
	Serial.println("Error: No se detecto movimiento en el tacometro.");
	return 0;
}

//------------------------------------------------------------------------------------------------------------------------------------------------

void loop() {
	temperatura = temperaturaTermistor(); // Lee la temperatura del termistor.
	verificarTemperatura(temperatura);	  // Verifica si la temperatura supera la temperatura de emergencia.
	pwm = calcularPWM(temperatura);		  // Calcula el valor de PWM basado en la temperatura
	setVelocidadPWM(pwm);				  // Setea el valor de PWM.
	delay(tiempoDeMuestreo);			  // Espera para volver a leer la temperatura.
	verificarVelocidad();				  // Verifica si el motor esta en movimiento si el tacometro esta activo.
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

	// Interpolación lineal
	for (int i = 0; i < cantElementosArray - 1; i++) { // TODO: Revisar si se pueden mejorar las variables.
		if (temperatura >= tempPWMArray[i].temperatura && temperatura <= tempPWMArray[i + 1].temperatura) {
			int tempDiff = tempPWMArray[i + 1].temperatura - tempPWMArray[i].temperatura;
			int pwmDiff = tempPWMArray[i + 1].porcentajePWM - tempPWMArray[i].porcentajePWM;
			int tempOffset = temperatura - tempPWMArray[i].temperatura;
			int porcentajePWM = tempPWMArray[i].porcentajePWM + (pwmDiff * tempOffset) / tempDiff;
			Serial.print("Porcentaje de PWM: ");
			Serial.print(porcentajePWM);
			Serial.println("%");
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
		setVelocidadPWM(pwmMax);		 // Setea el valor de PWM.
		digitalWrite(LED_BUILTIN, HIGH); // Enciende el LED incorporado.
		Serial.println("Advertencia: Temperatura de emergencia detectada. Encendiendo motor al maximo.");
		delay(10000);					// Delay de 10 segundos a maxima velocidad antes de la proxima lecturaTermistor.
		digitalWrite(LED_BUILTIN, LOW); // Apaga el LED incorporado.
	}
}

// Devuelve la velocidad actual del motor en RPM.
int velocidadActual() {
    pulsos = 0;                         // Cantidad de pulsos detectados.
    tiempoInicial = millis(); // Tiempo inicial.

    while (millis() - tiempoInicial < tiempoMaxEspera) { // Mientras no se cumpla el tiempo de espera.
        unsigned long tiempoInicioPulso = millis(); // Tiempo de inicio de la espera del pulso.
        bool pulsoDetectado = false;
        while (millis() - tiempoInicioPulso < tiempoMaxEsperaPulso) {
            if (digitalRead(pinTacometro) == LOW) { // Si detecta un pulso (borde de bajada).
                pulsoDetectado = true;
                break;
            }
        }
        if (pulsoDetectado) {
            pulsos++; // Incrementa la cantidad de pulsos.
            while (digitalRead(pinTacometro) == LOW) {
                // Espera a que el pulso termine.
            }
        }
        delay(1); // Agrega un pequeño retraso para evitar el WDT reset.
    }
    // Calcula la velocidad en RPM.
    // RPM = (pulsos / tiempoMedidoEnSegundos) * 60 / 2
    velocidad = (pulsos / ((millis() - tiempoInicial) / 1000.0)) * 60 / 2;

    Serial.print("Velocidad: ");
    Serial.print(velocidad);
    Serial.println(" RPM");
    return velocidad;
}

// Devuelve la temperatura del termistor.
int temperaturaTermistor() {
	lecturaTermistor = analogRead(pinTermistor); // lecturaTermistor del termistor.
	if (lecturaTermistor < 0 || lecturaTermistor > 1023) { // Si la lecturaTermistor esta fuera de rango, devuelve 0.
		Serial.println("Error: lecturaTermistor del termistor fuera de rango");
		return 0; // Valor por defecto en caso de error.
	}
	//float R = (tR1 * (1023.0 / lecturaTermistor - 1.0)); // Resistencia del termistor.
	// Serial.print("Resistencia: ");
	// Serial.print(R);
	// Serial.println(" ohms");
	temperatura = (1.0 / (1.0 / tT0 + log((tR1 * (1023.0 / lecturaTermistor - 1.0)) / tR0) / tBeta)) - kelvin ; // Temperatura en Kelvin.
	Serial.print("Temperatura: ");
	Serial.print(temperatura);
	Serial.println("°C");
	return temperatura; // Temperatura en Celsius.
}

// Setea la velocidad del motor en PWM.
void setVelocidadPWM(int velocidad) { // TODO: REVISAR si es suficiente o se necesita otros cambios de frecuencia.
	Serial.print("Velocidad: ");
	Serial.print(velocidad);
	Serial.println("%");
	analogWrite(pinPWM, map(velocidad, 0, 100, 0, 255));
}

// Verifica si el motor esta en movimiento si el tacometro esta activo.
void verificarVelocidad() {
	if (tacometro && pwm > pwmMin && !enMovimiento()) {
		erroresVelocidad++;
		while (!enMovimiento() && erroresVelocidad < erroresVelocidadMax) {
			setVelocidadPWM(pwmMax);
			delay(1000);
			if (enMovimiento()) {
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
