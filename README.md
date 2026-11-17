# Der Mondfisch

UCI compatible chess engine written in C++.

Playing on [lichess.org](https://lichess.org/@/DarkElectronBot) (accepting challenges).
Running on a Raspberry Pi 3.

# Features
- Search:
    - negamax
    - alphabeta pruning
    - iterative deepening
    - Transposition table
    - quiescence (no delta pruning)
    - principal variation search
    - killer heuristic
    - history heuristic
    - null move pruning
    - late move reduction
    - aspiration windows
    - reverse futility pruning

- Move ordering:
    - promotions
    - mvv - lva (see)
    - quiet moves (history, killers)

- Evaluation
    - static piece values
    - piece square tables with tapered eval

## Not implemented (maybe sometime in the future)
- more eval stuff
- magic bitboards (currently only quintessence)
- pinned pieces and attack tables (for faster move gen, less is_legal checks)
- quiescence with checks?

