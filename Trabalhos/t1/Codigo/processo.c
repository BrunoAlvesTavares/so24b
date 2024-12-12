#include "processo.h"
#include "console.h"
#include <stdlib.h>

// Função auxiliar para verificar preempção
static bool preemptiu(processo_t *self, estado_processo_t estado)
{
    return self->estado == ESTADO_INICIALIZANDO && estado == ESTADO_PRONTO;
}

processo_t *aloca_processo()
{
    processo_t *proc = malloc(sizeof(processo_t));
    if (proc == NULL)
    {
        console_printf("SO: erro ao alocar memória para o novo processo");
    }
    return proc;
}

void inicializa_processo(processo_t *proc, int pid, int pc)
{
    proc->pid = pid;
    proc->pc = pc;
    proc->estado = ESTADO_PRONTO;
    proc->reg[0] = 0;
    proc->reg[1] = 0;
    proc->prioridade = 0.5;

    proc->metricas.quantidade_preempcoes = 0;
    proc->metricas.tempo_retorno = 0;
    proc->metricas.tempo_resposta = 0;

    for (int i = 0; i < ESTADO_N; i++)
    {
        proc->metricas.estados[i].quantidade = 0;
        proc->metricas.estados[i].tempo_total = 0;
    }

    proc->metricas.estados[ESTADO_PRONTO].quantidade = 1;
}

void proc_muda_estado(processo_t *proc, estado_processo_t estado)
{
    if (preemptiu(proc, estado))
    {
        proc->metricas.quantidade_preempcoes++;
    }

    console_printf("Processo PID: %d, estado: %s -> %s\n", proc->pid, estado_processo_para_string(proc->estado), estado_processo_para_string(estado));
    proc->metricas.estados[estado].quantidade++;
    proc->estado = estado;
}
