# Record Types

## Standard Record Types

The following standard EPICS record types are supported:

| Record Type                 | Support |
| :-------------------------- | :------ |
| `ai` / `ao`                 | Analog input/output |
| `bi` / `bo`                 | Binary input/output |
| `longin` / `longout`        | 32-bit integer |
| `int64in` / `int64out`      | 64-bit integer (added in v0.4.0) |
| `mbbi` / `mbbo`             | Multi-bit binary input/output (enum) |
| `mbbiDirect` / `mbboDirect` | Multi-bit binary direct |
| `stringin` / `stringout`    | String data |
| `lsi` / `lso`               | Long string data |
| `waveform` / `aai` / `aao`  | Array data |

## Custom Record Types
*   `opcuaItem`:
    Used for structured data *(added in v0.3.0)*.
