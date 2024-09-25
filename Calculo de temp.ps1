# Definir las constantes
$T_BETA_ORIGINAL = 3950.0
$T_BETA_NUEVO = 4092.0
$T_BETA_INTERMEDIO = ($T_BETA_ORIGINAL + $T_BETA_NUEVO) / 2
$T_R0 = 100000.0
$T_T0 = 298.15
$resistenciaTermistor = 100000.0
$temperaturaBuscada = 30.0

# Función para calcular la temperatura
function Calcular-Temperatura {
    param (
        [float]$beta,
        [float]$resistencia
    )
    $temperaturaKelvin = 1.0 / (1.0 / $T_T0 + (1.0 / $beta) * [math]::Log($resistencia / $T_R0))
    $temperaturaCelsius = $temperaturaKelvin - 273.15
    return [math]::Round($temperaturaCelsius, 2)
}

# Función para calcular la resistencia del termistor para una temperatura dada
function Calcular-Resistencia {
    param (
        [float]$beta,
        [float]$temperaturaCelsius
    )
    $temperaturaKelvin = $temperaturaCelsius + 273.15
    $resistencia = $T_R0 * [math]::Exp($beta * (1.0 / $temperaturaKelvin - 1.0 / $T_T0))
    return [math]::Round($resistencia, 2)
}

# Mostrar las constantes
Write-Output "Constantes:"
Write-Output "Beta Original: $T_BETA_ORIGINAL"
Write-Output "Beta Nuevo: $T_BETA_NUEVO"
Write-Output "Beta Intermedio: $T_BETA_INTERMEDIO"
Write-Output "R0: $T_R0 ohmios"
Write-Output "T0: $T_T0 K"
Write-Output "Resistencia del termistor: $resistenciaTermistor ohmios"
Write-Output "Temperatura buscada: $temperaturaBuscada C"

# Calcular las temperaturas
$temperaturaOriginal = Calcular-Temperatura -beta $T_BETA_ORIGINAL -resistencia $resistenciaTermistor
$temperaturaNueva = Calcular-Temperatura -beta $T_BETA_NUEVO -resistencia $resistenciaTermistor
$temperaturaIntermedia = Calcular-Temperatura -beta $T_BETA_INTERMEDIO -resistencia $resistenciaTermistor

# Calcular la diferencia
$diferencia = [math]::Round($temperaturaNueva - $temperaturaOriginal, 2)

# Calcular las resistencias para la temperatura buscada
$resistenciaOriginal = Calcular-Resistencia -beta $T_BETA_ORIGINAL -temperaturaCelsius $temperaturaBuscada
$resistenciaNueva = Calcular-Resistencia -beta $T_BETA_NUEVO -temperaturaCelsius $temperaturaBuscada
$resistenciaIntermedia = Calcular-Resistencia -beta $T_BETA_INTERMEDIO -temperaturaCelsius $temperaturaBuscada

# Mostrar los resultados
Write-Output "Temperatura con Beta $T_BETA_ORIGINAL : $temperaturaOriginal C"
Write-Output "Temperatura con Beta $T_BETA_NUEVO : $temperaturaNueva C"
Write-Output "Temperatura con Beta Intermedio : $temperaturaIntermedia C"
Write-Output "Diferencia de temperatura: $diferencia C"

Write-Output ""

Write-Output "Resistencia con Beta $T_BETA_ORIGINAL para $temperaturaBuscada C: $resistenciaOriginal ohmios"
Write-Output "Resistencia con Beta $T_BETA_NUEVO para $temperaturaBuscada C: $resistenciaNueva ohmios"
Write-Output "Resistencia con Beta Intermedio para $temperaturaBuscada C: $resistenciaIntermedia ohmios"
Write-Output "% de Diferencia en resistencia: $([math]::Round(($resistenciaNueva - $resistenciaOriginal) / $resistenciaOriginal * 100, 2))%"

Write-Output ""

# For que muestra el desvio en grados que tendria el termistor de BETA_NUEVO si se usara el T_BETA_INTERMEDIO cada 5 grados
for ($i = 0; $i -le 100; $i += 5) {
    $resistenciaOriginal = Calcular-Resistencia -beta $T_BETA_INTERMEDIO -temperaturaCelsius $i
    $resistenciaNueva = Calcular-Resistencia -beta $T_BETA_NUEVO -temperaturaCelsius $i
    $temperaturaNueva = Calcular-Temperatura -beta $T_BETA_NUEVO -resistencia $resistenciaOriginal
    $diferenciaTemperatura = $temperaturaNueva - $i
    Write-Output "Desvio en $i grados: $diferenciaTemperatura C"
}

