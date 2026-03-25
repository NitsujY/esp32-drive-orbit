# Stage 4 Protocol Approval

## Status

Approved

## Date

2026-03-25

## Context

The project has completed the pure UI preview stage and now needs an explicit interaction-validation checkpoint before deeper firmware transport work continues.

The purpose of this record is to freeze the first-stage operating assumptions so Stage 5 implementation work does not mix unresolved architecture questions with parser and transport code.

## Decisions

1. The system remains local-first.
2. Wi-Fi is out of scope for the first implementation stage.
3. Inter-board transport remains deferred until later hardware and coexistence validation.
4. `companion_orb` remains the approved secondary display-oriented board.

## Consequences

- Stage 5 can focus on bounded implementation work, starting with parser behavior and shared protocol framing.
- No Stage 5 task should assume wireless transport exists yet.
- No Stage 5 task should make the secondary display a dependency for primary dashboard bring-up.
- Health, trip, and dashboard UI work should continue to treat the preview as the reference behavior until embedded rendering replaces it.

## Immediate Next Work

1. Keep `platformio.ini` and shared headers as the implementation baseline.
2. Implement the slave-side UART parser with partial-packet handling.
3. Validate parser behavior independently from any future inter-board transport choice.
