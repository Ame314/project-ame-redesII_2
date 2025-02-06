import socket

# Dirección IP y puerto del servidor ESP32
HOST = "10.10.65.70"  # Reemplaza con la IP del ESP32
PORT = 3333

while True:
    # Solicita al usuario un comando para encender o apagar el LED
    command = input("Escribe ON para encender el LED, OFF para apagarlo: ").strip().upper()
    
    # Verifica que el comando ingresado sea válido
    if command not in ["ON", "OFF"]:
        print("Comando inválido. Usa ON u OFF.")
        continue

    try:
        # Crea un socket TCP/IP
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((HOST, PORT))  # Conecta al servidor ESP32
            s.sendall(command.encode())  # Envía el comando al ESP32
            response = s.recv(1024).decode()  # Recibe la respuesta del ESP32
            print("Respuesta del servidor:", response)  # Muestra la respuesta recibida
    except Exception as e:
        print("Error de conexión:", e)  # Maneja posibles errores de conexión
