
# Sift

Sift is a superhuman chess engine written in C++.

It implements bitboard-based legal move generation, alpha-beta search with numerous optimization techniques, and an Effeciently Updatable Neural Network (NNUE) for evaluation.

_This project does not contain any code written by generative-AI models._

## Strength

TODO

## Building From Source

Requirements: `make`, `clang++`, `lld`.

To build a binary tuned for your specific CPU, run:

``` sh
make -j
```

To build for a specific architecture, pass `ARCH=` followed by one of the options below:

- `native` (default): tuned for the current CPU
- `avx512`: requires BMI2, AVX-512, VNNI and VBMI2
- `avx2_bmi2`: requires BMI2 and AVX2, and assumes fast PEXT
- `zen2`: requires BMI and AVX2, but doesn't assume fast PEXT
- `armv8_4`: requires NEON with dotprod
- `apple_m1`: requires NEON with dotprod for Apple silicon devices

For Linux machines, optimizations for non-uniform memory access (NUMA) are supported with libnuma, which can be enabled by passing `NUMA=on`.

## Usage

Sift is compliant with the [UCI protocol](https://gist.github.com/DOBRO/2592c6dad754ba67e6dcaec8c90165bf), meaning it is a command-line program and does not include its own GUI. It can be used through any third-party chess GUI, such as  [En Croissant](https://encroissant.org/), [Cute Chess](https://cutechess.com/), [Nibbler](https://github.com/rooklift/nibbler), and [BanksiaGUI](https://banksiagui.com/).

### Standard UCI Commands

- `uci`
- `isready`
- `setoption name <id> [value <x>]`
- `ucinewgame`
- `position [fen <fenstring> | startpos] moves <move1> .... <movei>`
- `go`
  - `wtime <x>`
  - `btime <x>`
  - `winc <x>`
  - `binc <x>`
  - `movestogo <x>`
  - `depth <x>`
  - `nodes <x>`
  - `movetime <x>`
  - `infinite`
- `stop`
- `quit`

### Custom UCI Commands

- `bench <x>`: run searches of depth `x` (with the current options) on a fixed set of internal positions
- `perft <x>`: run a bulk-perft search of depth `x` on the current position
- `perfttests`: run bulk-perft searches on a fixed set of internal positions
- `board`, `d`: display the current position
- `moves`: display the legal moves for the current position
- `fen`: display the fen-string for the current position
- `hash`: display the internal hash for the current position
- `eval`: display the normalized static evaluation for the current position
- `raweval`: display the raw neural-network static evaluation for the current position
- `help`, `about`: display a short info message

### UCI Options

| **Name**       | **Type** | **Default** | **Range**       | **Description**                                                                   |
|----------------|----------|-------------|-----------------|-----------------------------------------------------------------------------------|
| `Hash`         | Spin     | 64          | [1, 33554432]   | Memory allocated to the internal transposition table (in MiB).                       |
| `ClearHash`    | Button   | N/A         | N/A             | Resets all internal state; equivalent to the  `ucinewgame` command.               |
| `Threads`      | Spin     | 1           | [1, 2048]       | Number of threads used to search.                                                 |
| `MultiPV`      | Spin     | 1           | [1, 256]        | Number of lines to search at once.                                                |
| `Contempt`     | Spin     | 0           | [-1000, 1000]   | Static offset applied to all evaluations; roughly equates to the value of a draw. |
| `MoveOverhead` | Spin     | 10          | [0, 5000]       | Amount of time assumed to be lost to overhead per move (in ms).                      |
| `ShowWDL`      | Check    | `true`      | `false`, `true` | Display predicted win/draw/loss probabilities in search output.                   |

## Credit

Sift uses the [incbin](https://github.com/graphitemaster/incbin) library for embedding of neural network files within the engine binary.

Other tools used throughout Sift's development include:

- [fastchess](https://github.com/Disservin/fastchess), for running local SPRT tests
- [OpenBench](https://github.com/AndyGrant/OpenBench), for running SPSA parameter tuning
- [Weather Factory](https://github.com/jnlt3/weather-factory), also for running SPSA parameter tuning
- [WDL_model](https://github.com/official-stockfish/WDL_model), for calculating the win/draw/loss probabilities displayed during search
- [Cute Chess](https://cutechess.com/), for playing against the engine and displaying matches
- [BanksiaGUI](https://banksiagui.com/), also for playing against the engine and displaying matches

## Other Acknowledgements

Due to personal hardware limitations, Sift uses neural networks from other chess engines. They can be found in [Sift-Nets](https://github.com/andrewharabor/Sift-Nets/releases), and are credited there accordingly.

Finally, this project would not be possible without the many sources of inspiration and information that greatly helped Sift's development (in no particular order):

- [Stockfish](https://github.com/official-stockfish/Stockfish)
- [Stormphrax](https://github.com/Ciekce/Stormphrax)
- [Raphael](https://github.com/Orbital-Web/Raphael)
- [Sirius](https://github.com/mcthouacbb/Sirius)
- [PlentyChess](https://github.com/Yoshie2000/PlentyChess)
- [Halogen](https://github.com/KierenP/Halogen)
- [Potential](https://github.com/ProgramciDusunur/Potential)
- [Hobbes](https://github.com/kelseyde/hobbes-chess-engine)
- [pawnocchio](https://github.com/JonathanHallstrom/pawnocchio)
- [Integral](https://github.com/aronpetko/integral)
- [Reckless](https://github.com/codedeliveryservice/Reckless)
- [Stash](https://github.com/mhouppin/stash-bot)
- [Marvin](https://github.com/bmdanielsson/marvin-chess)
- [Sebastian Lague's Chess Coding Adventures series](https://www.youtube.com/watch?v=U4ogK0MIzqk&list=PLFt_AvWsXl0cvHyu32ajwh2qU1i6hl77c)
- [chess-library](https://github.com/Disservin/chess-library)
- [87flowers.com](https://87flowers.com/)
- [asteri.sm](https://asteri.sm/)
- [Chess Programming Wiki](https://chessprogramming.org/)
- Stockfish Discord server
- Alpha-Beta Discord server
