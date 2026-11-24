# VitaPlug LAB2576C · 0.1.2

Compresor de laboratorio que combina un VCA con un FET mediante el control MORPH. Sigue el espíritu y el diseño de LAB BUNDLE de VitaPlug.

Esta primera versión explora qué cambia al pasar de un detector RMS a uno de pico en una misma cadena de audio. El motor C++ funciona separado del plugin y del editor.

El frontal del **Lab Bundle** usa el chasis LAB de VitaPlug y el perfil de sus mandos satinados. La pantalla alterna entre **IN/OUT** y **GR** y muestra un historial de dos segundos junto a las tres barras. **IN** activa la compresión y el frontal ofrece pulsadores separados para **FEED-FWD** y **FEEDBACK**. El mando **MORPH** recorre VCA y FET con una lectura porcentual.

En la primera escucha en Logic, SerxDev confirmó la diferencia de carácter entre VCA y FET, destacó el resultado de mezclarlos y no oyó clics. La revisión 0.1.2 conserva el motor de esa sesión y los IDs del Audio Unit y de sus parámetros.

| Ajuste | Comportamiento |
| --- | --- |
| **MORPH 0 %** | VCA con detector RMS. **VCA DETECTOR** selecciona feed-forward o feedback. |
| **MORPH 50 %** | Media de las dos reducciones en dB. |
| **MORPH 100 %** | FET con detector de pico, knee más estrecho y recuperación más lenta después de una reducción sostenida. |
| **SC HPF / SC TILT** | Ajustan cuánto contribuyen los graves al detector interno. |
| **LINK** | A 100 %, ambos canales reciben la mayor de las dos reducciones. A 0 %, cada canal conserva su propia reducción. |
| **MIX / OUTPUT** | Mezcla la entrada original con la rama comprimida. OUTPUT controla la salida de la mezcla. |
| **BYPASS** | Funde con la entrada original en 8 ms y conserva la actividad de los detectores. |

MORPH interpola una sola ganancia. El extremo FET usa realimentación para el cálculo de la reducción y el ajuste del release. El rango de ataque compartido va de 0,02 a 100 ms. La primera versión se centra en la dinámica y deja abierta la elección de la etapa de coloración. Admite mono y estéreo y trabaja sin lookahead. Los números de IN y OUT son picos en dBFS. El medidor GR muestra la reducción en la rama comprimida. El estado del proyecto guarda parámetros y tamaño del editor.

## Audio Unit para Logic en macOS

Con las herramientas de compilación de Apple y CMake 3.22 o posterior disponibles, abre Terminal en la carpeta del proyecto y ejecuta este comando con Logic cerrado.

```bash
bash scripts/instalar-au-macos.sh
```

El script compila y prueba el DSP, comprueba la presencia de las dos arquitecturas en el Audio Unit, lo firma localmente e instala `LAB2576C.component` en `~/Library/Audio/Plug-Ins/Components`. Si ya hay una versión con ese nombre, la guarda dentro de `build-au` antes de instalar la nueva. Al final consulta los identificadores del AU instalado y ejecuta `auval`. Si el AU todavía no aparece en la lista del sistema, reinicia el registro de Audio Units de tu usuario antes de validarlo. Para reutilizar un árbol de fuentes JUCE 8 local, pasa su ruta como argumento al script.

## VST3 para Windows x64

Abre PowerShell en la carpeta del proyecto en un equipo con Visual Studio y herramientas de escritorio C++. Usa CMake 3.22 o posterior.

```powershell
cmake -S . -B build-win -A x64
cmake --build build-win --config Release --parallel 4
ctest --test-dir build-win -C Release --output-on-failure
```

El resultado se genera en `build-win/LAB2576C_artefacts/Release/VST3/LAB2576C.vst3`. CMake usa JUCE 8.0.12 al descargar la dependencia. Para usar una copia local, añade `-DLAB2576C_JUCE_PATH=...` al configurar. [Documentación de CMake de JUCE](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md)

## Validación del AU 0.1.2 en Logic Pro

El 24 de noviembre de 2026, SerxDev probó la actualización en Logic Pro con el loop «60s Songwriter Drums 05», a 48 kHz y con un búfer de 512 muestras. En escucha, el compresor se comportó igual que la versión anterior y se integró bien con el resto de la cadena. Funcionaron la automatización y las acciones de deshacer y rehacer. La actualización recuperó los parámetros y el tamaño de ventana de la versión anterior, y ambos se conservaron tras cerrar y volver a abrir. Durante la reproducción se comprobó la actividad de la gráfica de GR y de las barras IN, OUT y GR.

## Primera escucha en Logic

1. Inserta LAB2576C en una caja. Empieza con RATIO 4:1, ATTACK 3 ms, RELEASE 120 ms y MIX 100 %. Baja THRESHOLD hasta que los golpes fuertes marquen unos 4–6 dB de reducción.
2. Recorre MORPH de 0 a 50 y 100 %. Compara el golpe y la recuperación entre notas. Iguala el volumen con OUTPUT al hacer A/B.
3. En un bus de batería estéreo, prueba LINK al 100 % y SC HPF entre OFF y 120 Hz. Escucha si el bombo condiciona cuánto cae el bus y si la imagen se mantiene centrada.
4. Alterna **IN/OUT** y **GR** durante la reproducción. Comprueba la gráfica, las barras y la respuesta al parar y volver a reproducir.
5. Automatiza MORPH y alterna **IN** durante la reproducción. Después guarda la sesión, ciérrala y comprueba los valores al abrirla.

## Comprobar el DSP por separado

```sh
cmake -S . -B build-dsp -DLAB2576C_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --config Release
ctest --test-dir build-dsp -C Release --output-on-failure
```

Las pruebas cubren la curva de ratio de cada topología, MORPH fijo y automatizado, enlace estéreo, filtrado del detector, recuperación FET, mezcla en paralelo, bypass y condiciones extremas entre 44,1 y 192 kHz. La validación en el DAW se realiza con cada formato compilado para su plataforma.

## Referencias sonoras

La conmutación del detector VCA toma como referencia el API 2500. La respuesta rápida y el release dependiente de programa toman como referencia el 1176. [API 2500](https://apiaudio.com/docs/brochures/br2500.pdf) · [Universal Audio 1176](https://help.uaudio.com/hc/en-us/articles/4419447352980-UA-1176-Classic-Limiter-Collection-Manual)
