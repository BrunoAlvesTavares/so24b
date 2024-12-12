#ifndef PROCESSOS_H
#define PROCESSOS_H

#include <stdbool.h>
#include "so.h"

typedef enum {
    ESTADO_INICIALIZANDO,
    ESTADO_PRONTO,
    ESTADO_BLOQUEADO,
    ESTADO_TERMINADO,
    ESTADO_N
} estado_processo_t;

typedef enum {
    BLOQUEIO_POR_LEITURA,
    BLOQUEIO_POR_ESCRITA,
    BLOQUEIO_POR_ESPERA_DE_PROC,
} motivo_bloq_processo_t;

typedef struct metricas_estado_processo_t {
    int quantidade;
    int tempo_total;
} metricas_estado_processo_t;

typedef struct processo_metricas_t {
    int quantidade_preempcoes;
    int tempo_retorno;
    int tempo_resposta;
    metricas_estado_processo_t estados[ESTADO_N];
} processo_metricas_t;

typedef struct processo_t {
    int pid;
    int pc;
    estado_processo_t estado;
    motivo_bloq_processo_t motivo_bloqueio;
    int reg[2];
    float prioridade;
    int dado_pendente;
    processo_metricas_t metricas;
} processo_t;

// Declarações de funções relacionadas a processos
processo_t *aloca_processo();
void inicializa_processo(processo_t *proc, int pid, int pc);
void proc_muda_estado(processo_t *proc, estado_processo_t estado);
const char *estado_processo_para_string(estado_processo_t estado);

#endif // PROCESSOS_H
