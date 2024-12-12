#ifndef SO_H
#define SO_H

#include "memoria.h"
#include "cpu.h"
#include "es.h"
#include "console.h"

// Declaração antecipada para evitar dependência circular
typedef struct processo_t processo_t;

#define INTERVALO_INTERRUPCAO 20
#define QUANTUM 5
#define ESCALONADOR 2 // 1 para prioridade, 2 round-robin, 3 para simples
#define QTD_IRQ 6     // qtd de interrupção

typedef struct no_fila_t {
    processo_t *processo;
    struct no_fila_t *proximo;
} no_fila_t;

typedef struct {
    no_fila_t *inicio;
    no_fila_t *fim;
} fila_t;

typedef struct {
    int tempo_total_execucao;
    int tempo_total_ocioso;
    int num_interrupcoes[QTD_IRQ];
    int num_preempcoes;
} so_metricas_t;

typedef struct so_t {
    cpu_t *cpu;
    mem_t *mem;
    es_t *es;
    console_t *console;
    bool erro_interno;
    processo_t *processo_corrente;
    processo_t **processos;
    fila_t *fila_prontos;
    int quantum_proc;
    int pid_atual;
    so_metricas_t metricas;
    int numero_processos;
    int relogio_atual;
} so_t;

// Declarações de funções do sistema operacional
so_t *so_cria(cpu_t *cpu, mem_t *mem, es_t *es, console_t *console);
void so_destroi(so_t *self);


// Chamadas de sistema
// Uma chamada de sistema é realizada colocando a identificação da
//   chamada (um dos valores abaixo) no registrador A e executando a
//   instrução CHAMAS, que causa uma interrupção do tipo IRQ_SISTEMA.

// Chamadas para entrada e saída
// Cada processo tem um dispositivo (ou arquivo) corrente de entrada
//   e um de saída. As chamadas de sistema para leitura e escrita são
//   realizadas nesses dispositivos.
// Outras chamadas (não definidas) abrem e fecham arquivos, e definem
//   qual dos arquivos abertos é escolhido para ser o de entrada ou
//   saída correntes.


// lê um caractere do dispositivo de entrada do processo
// retorna em A: o caractere lido ou um código de erro negativo
#define SO_LE          1

// escreve um caractere no dispositivo de saída do processo
// recebe em X o caractere a escrever
// retorna em A: 0 se OK ou um código de erro negativo
#define SO_ESCR        2

// #define SO_ABRE        3
// #define SO_FECHA       4
// #define SO_SEL_LE      5
// #define SO_SEL_ESCR    6


// Chamadas para gerenciamento de processos
// O sistema cria um processo automaticamente na sua inicialização,
//   para executar um programa inicial. Esse processo deve criar
//   outros processos para executar outros programas, se for o caso.
// Existem chamadas de sistema para criar um processo, para matar um
//   processo e para um processo se matar.
// Cada processo é identificado por um número (pid). O processo criado
//   automaticamente tem o pid 1, o segundo processo criado tem o
//   pid 2 etc.

// cria um processo novo, para executar um determinado programa
// os caracteres que compõem o nome do arquivo que contém o programa
//   a ser executado pelo novo processo estão na memória do processo
//   que realiza esta chamada, a partir da posição em X até antes
//   da posição que contém um valor 0.
// retorna em A: pid do processo criado, ou código de erro negativo
#define SO_CRIA_PROC   7

// mata um processo
// recebe em X o pid do processo a matar ou 0 para o processo chamador
// retorna em A: 0 se OK ou um código de erro negativo
#define SO_MATA_PROC   8

// espera um processo terminar
// recebe em X o pid do processo a esperar
// retorna em A: 0 se OK ou um código de erro negativo
// bloqueia o processo chamador até que o processo com o pid informado termine
// retorna sem bloquear, com erro, se não existir processo com esse pid
#define SO_ESPERA_PROC 9

#endif // SO_H
