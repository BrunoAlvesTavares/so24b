#include "metrica.h"
#include "processo.h"
#include "console.h"
#include "so.h"
#include <stdio.h>

// Inicializa as métricas do sistema operacional
void inicializa_metricas(so_t *self) {
    self->metricas.tempo_total_execucao = 0;
    self->metricas.tempo_total_ocioso = 0;
    self->metricas.num_preempcoes = 0;

    for (int i = 0; i < QTD_IRQ; i++) {
        self->metricas.num_interrupcoes[i] = 0;
    }
}

// Salva as métricas relacionadas às interrupções
static void salva_metricas_interrupcoes(FILE *file, const so_t *self) {
    fprintf(file, "\nINTERRUPÇÕES:\n");
    fprintf(file, "| %-5s | %-10s |\n", "IRQ", "QUANTIDADE");
    fprintf(file, "|-------|------------|\n");

    for (int i = 0; i < QTD_IRQ; i++) {
        fprintf(file, "| %-5d | %-10d |\n", i, self->metricas.num_interrupcoes[i]);
    }
}

// Salva as métricas de um processo específico
static void salva_metricas_processo(FILE *file, const processo_t *proc) {
    fprintf(file, "\nPROCESSO %d\n", proc->pid);
    fprintf(file, "| %-23s | %-10s |\n", "MÉTRICA", "VALOR");
    fprintf(file, "|------------------------|------------|\n");
    fprintf(file, "| NÚMERO DE PREEMPÇÕES   | %-10d |\n", proc->metricas.quantidade_preempcoes);
    fprintf(file, "| TEMPO DE RESPOSTA      | %-10d |\n", proc->metricas.tempo_resposta);
    fprintf(file, "| TEMPO DE RETORNO       | %-10d |\n", proc->metricas.tempo_retorno);

    fprintf(file, "\nESTADOS DO PROCESSO %d:\n", proc->pid);
    fprintf(file, "| %-12s | %-10s | %-12s |\n", "ESTADO", "QUANTIDADE", "TEMPO TOTAL");
    fprintf(file, "|------------|------------|--------------|\n");

    for (int j = 0; j < ESTADO_N; j++) {
        fprintf(file, "| %-12s | %-10d | %-12d |\n",
                estado_processo_para_string(j),
                proc->metricas.estados[j].quantidade,
                proc->metricas.estados[j].tempo_total);
    }
}

// Salva todas as métricas do sistema e processos no arquivo especificado
void so_salva_metricas(so_t *self, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        console_printf("Erro ao abrir o arquivo %s para escrita.\n", filename);
        return;
    }

    fprintf(file, "MÉTRICAS DO SISTEMA OPERACIONAL\n");
    fprintf(file, "| %-30s | %-10s |\n", "MÉTRICA", "VALOR");
    fprintf(file, "|-------------------------------|------------|\n");
    fprintf(file, "| NÚMERO DE PROCESSOS           | %-10d |\n", self->numero_processos);
    fprintf(file, "| TEMPO TOTAL DE EXECUÇÃO       | %-10d |\n", self->metricas.tempo_total_execucao);
    fprintf(file, "| TEMPO TOTAL OCIOSO            | %-10d |\n", self->metricas.tempo_total_ocioso);
    fprintf(file, "| NÚMERO DE PREEMPÇÕES          | %-10d |\n", self->metricas.num_preempcoes);

    salva_metricas_interrupcoes(file, self);

    fprintf(file, "\nMÉTRICAS DOS PROCESSOS:\n");
    for (int i = 0; i < self->numero_processos; i++) {
        salva_metricas_processo(file, self->processos[i]);
    }

    fclose(file);
    console_printf("Métricas salvas no arquivo %s com sucesso.\n", filename);
}
