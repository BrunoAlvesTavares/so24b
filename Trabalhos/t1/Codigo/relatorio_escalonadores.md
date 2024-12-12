
# Relatório Experimental

---

## Título do Experimento
Análise Comparativa de Escalonadores em um Sistema Operacional Simulado

## Objetivo
O objetivo deste experimento é avaliar o desempenho de três diferentes técnicas de escalonamento (Prioridade, Round-Robin e Simples) em um sistema operacional simulado. Além disso, busca-se documentar as implementações realizadas para atender aos requisitos do trabalho, abordando aspectos como gerenciamento de processos, controle de estados e métricas de desempenho.

---

## Implementação do Sistema Operacional

### Estruturas Criadas
1. **Estrutura `proc_t`**:
   - Armazena informações essenciais de cada processo, incluindo:
     - Registradores.
     - Estado atual (pronto, executando, bloqueado ou morto).
     - Métricas como tempo de resposta, retorno e número de preempções.
   - Inclui funções auxiliares para inicialização e alteração de estados.

2. **Estrutura `esc_t`**:
   - Representa o escalonador, com uma fila para processos prontos e lógica de seleção do próximo processo a ser executado.
   - Implementa três estratégias distintas de escalonamento:
     - **Simples**: Escolhe o primeiro processo pronto e executa até bloqueio ou finalização.
     - **Round-Robin**: Alterna processos com base em um quantum fixo.
     - **Prioridade**: Processos de maior prioridade são selecionados com base em um mecanismo dinâmico de ajuste de prioridades.

3. **Estrutura `so_t`**:
   - Centraliza o controle do sistema operacional, gerenciando:
     - Interações entre o escalonador e os processos.
     - Chamadas de sistema (criação, espera e finalização de processos).
     - Coleta de métricas detalhadas de desempenho.

### Chamadas de Sistema
Foram implementadas chamadas para:
- **Leitura e Escrita**: Operar com dispositivos de entrada e saída.
- **Gerenciamento de Processos**:
  - **SO_CRIA_PROC**: Cria um novo processo.
  - **SO_ESPERA_PROC**: Aguarda o término de um processo específico.
  - **SO_MATA_PROC**: Finaliza um processo.

---

## Testes Realizados

### Configuração do Ambiente
Os testes foram conduzidos com quatro processos de teste, variando em requisitos de CPU e E/S. Os cenários foram configurados com diferentes valores de `INTERVALO_RELOGIO` e `INTERVALO_QUANTUM`, conforme a estratégia de escalonamento.

### Resultados Obtidos

#### Métricas do Sistema
| Métrica                  | Prioridade | Round-Robin | Simples  |
|--------------------------|------------|-------------|----------|
| **Tempo Total de Execução** | 21905      | 21917       | 27211    |
| **Tempo Ocioso**          | 393        | 405         | 6056     |
| **Número de Preempções**   | 129        | 146         | 0        |

#### Métricas por Processo
| Escalonador  | Processo | Tempo de Resposta | Tempo de Retorno | Número de Preempções |
|--------------|----------|-------------------|------------------|--------------------|
| Prioridade   | 1        | 0                 | 21905            | 0                  |
| Prioridade   | 2        | 125               | 21114            | 74                 |
| Prioridade   | 3        | 96                | 22004            | 39                 |
| Prioridade   | 4        | 59                | 21600            | 16                 |
| Round-Robin  | 1        | 0                 | 21917            | 0                  |
| Round-Robin  | 2        | 96                | 21126            | 98                 |
| Round-Robin  | 3        | 152               | 21804            | 32                 |
| Round-Robin  | 4        | 84                | 21706            | 16                 |
| Simples      | 1        | 0                 | 27211            | 0                  |
| Simples      | 2        | 649               | 14322            | 0                  |
| Simples      | 3        | 503               | 15775            | 0                  |
| Simples      | 4        | 106               | 27167            | 0                  |

---

## Discussão

### 1. Escalonador Simples
- Escolhe o primeiro processo pronto e o executa até bloqueio ou término.
- Resultados indicaram alto tempo ocioso (6056) devido à falta de preempção, tornando-o inadequado para ambientes concorrentes.
- Apropriado apenas para cargas leves e sem a necessidade de troca de contexto frequente.

### 2. Round-Robin
- Altera processos com base em um quantum fixo, promovendo uma distribuição mais justa de CPU.
- Apresentou maior número de preempções (146), o que pode impactar o desempenho em hardwares modernos devido à limpeza do pipeline.
- Adequado para sistemas interativos, onde responsividade é essencial.

### 3. Escalonador de Prioridade
- Favorece processos de maior prioridade, ajustando dinamicamente os valores.
- Demonstrou o melhor desempenho em termos de tempo total de execução (21905) e ociosidade (393), mas pode penalizar processos de baixa prioridade.
- Ideal para sistemas críticos, onde a execução de tarefas prioritárias é essencial.

### Impacto Geral
Os resultados confirmam que cada estratégia apresenta vantagens e desvantagens:
- **Prioridade**: Maior eficiência em sistemas críticos.
- **Round-Robin**: Melhor justiça para distribuição de CPU.
- **Simples**: Adequado apenas para cenários de baixa concorrência.

---

## Conclusão

O experimento demonstrou que as implementações realizadas atenderam aos requisitos do trabalho, abrangendo o gerenciamento de processos, controle de estados e coleta de métricas detalhadas. A escolha do escalonador deve levar em consideração as características do sistema-alvo, equilibrando eficiência, justiça e simplicidade. A separação clara de responsabilidades entre as estruturas `proc_t`, `esc_t` e `so_t` contribuiu para um sistema flexível e extensível.
