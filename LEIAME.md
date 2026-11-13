# Choir Crowd (estilo Asake)

## Obter o instalador (.pkg) sem instalar nada no Mac

Os plugins para Mac só podem ser compilados num Mac. Em vez de usares o teu, o GitHub compila-o gratuitamente e entrega-te o instalador pronto:

1. Criar uma conta gratuita em github.com e um repositório novo (privado serve).
2. Carregar o conteúdo desta pasta para o repositório (Add file > Upload files). A pasta oculta `.github` tem de ir também; se não aparecer, cria o ficheiro em Add file > Create new file com o nome `.github/workflows/instalador-mac.yml` e cola lá o conteúdo.
3. Separador Actions > "Instalador Mac" > Run workflow. Demora cerca de 10 a 15 minutos.
4. Quando terminar, descarregar o artefacto "Choir-Crowd-Instalador" (um zip com o .pkg).
5. Abrir o .pkg e seguir o assistente (pede a palavra-passe do Mac). Se o macOS bloquear por não ser de um programador identificado: clicar com o botão direito no .pkg > Abrir.
6. Reiniciar o Logic Pro. O plugin aparece em AU Instruments > CrowdLab > Choir Crowd.

O instalador funciona em Macs Apple Silicon e Intel e instala o AU (Logic Pro) e o VST3 (outras DAWs).

---

## Compilar manualmente (opcional)

Instrumento (AU, VST3 e Standalone) que gera um coro de multidão: dezenas de vozes sintetizadas com afinação, vibrato, entrada e posição estéreo ligeiramente diferentes, filtradas por formantes vocais (Ah, Eh, Ee, Oh, Oo), com ensemble e reverb de sala grande.

## Compilar no MacBook Pro

1. Instalar as ferramentas: `xcode-select --install` e `brew install cmake`.
2. Na pasta do projeto:

```
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```

O JUCE é descarregado automaticamente na primeira execução. No fim, o plugin é copiado para:

- `~/Library/Audio/Plug-Ins/Components/` (Audio Unit, para o Logic Pro)
- `~/Library/Audio/Plug-Ins/VST3/` (Ableton, FL Studio, Reaper, Cubase, etc.)

## Usar no Logic Pro

1. Reiniciar o Logic. Se pedir, validar o plugin (Logic Pro > Definições > Plug-ins).
2. Criar uma pista de instrumento de software e escolher o Choir Crowd em AU Instruments > CrowdLab.
3. Tocar acordes de 3 a 4 notas, na região C3–C5.

Se o Logic não o mostrar: `killall -9 AudioComponentRegistrar` e reabrir.

## Receita para o som tipo Asake

| Parâmetro | Valor |
|---|---|
| Crowd Size | 24–32 |
| Vowel | Oh (3) ou Oo (4) |
| Octave Layer | 0.4–0.6 (vozes graves) |
| Detune / Humanize | 18–25 cents / 0.6 |
| Attack / Release | 0.08 s / 0.8 s |
| Ensemble / Reverb Mix | 0.4 / 0.35 |

Dicas: toca notas curtas e sincopadas para o efeito de "gang vocal"; faz bounce e usa o Flex Time ou o Sampler para picotar frases; sobrepõe com um coro gravado de verdade para o realismo.

## Parâmetros

Crowd Size, Vowel, Octave Layer, Detune, Vibrato, Humanize, Stereo Spread, Breath, Attack, Release, Ensemble, Reverb Size, Reverb Mix, Low Cut, Output.

## Nota

É um coro sintetizado: soa a "ooh/aah" de multidão, não a vozes gravadas a cantar palavras em ioruba. O nível de saída pode precisar de ajuste (parâmetro Output).
