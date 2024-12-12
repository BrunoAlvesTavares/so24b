// so.c
// sistema operacional
// simulador de computador
// so24b

// INCLUDES {{{1
#include "so.h"
#include "dispositivos.h"
#include "irq.h"
#include "programa.h"
#include "instrucao.h"
#include "metrica.h"
#include "processo.h"

#include <stdlib.h>
#include <stdbool.h>

/**
 * @brief Retorna uma string representando o estado de um processo.
 * 
 * Essa função converte um valor do tipo `estado_processo_t` para sua 
 * representação textual. Caso o estado não seja reconhecido, retorna 
 * uma mensagem padrão indicando que o estado não foi tratado.
 * 
 * @param estado O estado do processo, do tipo `estado_processo_t`.
 * @return const char* Uma string representando o nome do estado.
 */
const char *estado_processo_para_string(estado_processo_t estado)
{
    // Usamos um switch para mapear os valores enum para strings.
    switch (estado)
    {
        case ESTADO_PRONTO:       // Quando o processo está na fila, aguardando execução.
            return "PRONTO";

        case ESTADO_INICIALIZANDO:  // Quando o processo está atualmente em execução.
            return "EXECUTANDO";

        case ESTADO_BLOQUEADO:   // Quando o processo está aguardando recursos ou evento.
            return "BLOQUEADO";

        case ESTADO_TERMINADO:       // Quando o processo foi encerrado.
            return "MORTO";

        default:                 // Para estados não tratados explicitamente no switch.
            return "NÃO TRATADO";
    }
}
/**
 * @brief Atualiza as métricas de um processo específico.
 * 
 * Essa função atualiza as métricas relacionadas ao processo, como:
 * - Tempo de retorno: Incrementado a cada atualização enquanto o processo não está morto.
 * - Tempo total em cada estado: Incrementado com base no estado atual do processo.
 * - Tempo de resposta médio: Calculado como o tempo total no estado PRONTO dividido pela quantidade de vezes que o processo esteve nesse estado.
 * 
 * Também imprime no console informações de depuração sobre o processo, incluindo:
 * - PID do processo.
 * - Tempo total acumulado no estado atual.
 * - Nome do estado atual.
 * 
 * @param self Ponteiro para o processo a ser atualizado.
 * @param dif_tempo Diferença de tempo desde a última atualização.
 */
static void atualiza_metricas_processo(processo_t *self, int dif_tempo) {
    // Incrementa o tempo de retorno se o processo não está morto
    if (self->estado != ESTADO_TERMINADO) {
        self->metricas.tempo_retorno += dif_tempo;
    }

    // Incrementa o tempo total acumulado no estado atual
    self->metricas.estados[self->estado].tempo_total += dif_tempo;

    // Atualiza o tempo de resposta médio (tempo em estado PRONTO dividido pelo número de entradas nesse estado)
    if (self->metricas.estados[ESTADO_PRONTO].quantidade > 0) {
        self->metricas.tempo_resposta = self->metricas.estados[ESTADO_PRONTO].tempo_total /
                                        self->metricas.estados[ESTADO_PRONTO].quantidade;
    }

    // Imprime informações de depuração no console
    console_printf(
        "Processo PID: %d, Tempo: %d, Estado: %s\n",
        self->pid,
        self->metricas.estados[self->estado].tempo_total,
        estado_processo_para_string(self->estado)
    );
}

/**
 * @brief Atualiza as métricas do sistema operacional e dos processos.
 * 
 * Essa função realiza as seguintes atualizações:
 * - Incrementa o tempo total de execução do sistema.
 * - Incrementa o tempo ocioso do sistema se nenhum processo estiver em execução.
 * - Atualiza as métricas individuais de cada processo.
 * 
 * @param self Ponteiro para o sistema operacional (`so_t`).
 * @param dif_tempo Diferença de tempo desde a última atualização.
 */
static void atualiza_metricas_sistema(so_t *self, int dif_tempo) {
    // Incrementa o tempo total de execução do sistema
    self->metricas.tempo_total_execucao += dif_tempo;

    // Verifica se o sistema está ocioso (nenhum processo em execução)
    if (self->processo_corrente == NULL) {
        self->metricas.tempo_total_ocioso += dif_tempo;
    }

    // Atualiza as métricas para cada processo
    if (self->numero_processos > 0) { // Evita processamento desnecessário se não houver processos
        for (int i = 0; i < self->numero_processos; i++) {
            atualiza_metricas_processo(self->processos[i], dif_tempo);
        }
    }
}

// função de tratamento de interrupção (entrada no SO)
static int so_trata_interrupcao(void *argC, int reg_A);

// busca o terminal correspondente ao PID
static int obter_terminal_por_pid(int pid);

// funções auxiliares
// carrega o programa contido no arquivo na memória do processador; retorna end. inicial
static int so_carrega_programa(so_t *self, char *nome_do_executavel);
// copia para str da memória do processador, até copiar um 0 (retorna true) ou tam bytes
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender);

// CRIAÇÃO {{{1

/**
 * @brief Configura e inicializa a fila de processos prontos no sistema operacional.
 * 
 * Esta função aloca memória para a estrutura `fila_t`, que gerencia os processos prontos
 * para execução. Os ponteiros `inicio` e `fim` da fila são configurados como `NULL` para
 * indicar que a fila está inicialmente vazia.
 * Caso a alocação falhe, registra uma mensagem de erro e marca o sistema operacional
 * como estando em um estado de erro interno.
 * 
 * @param self Ponteiro para o sistema operacional (`so_t`) que contém a fila de prontos.
 */
static void configura_fila_prontos(so_t *self) {
    // Aloca memória para a estrutura da fila de processos prontos
    self->fila_prontos = malloc(sizeof(fila_t));

    // Verifica se a alocação de memória foi bem-sucedida
    if (self->fila_prontos == NULL) {
        // Registra uma mensagem de erro no sistema
        console_printf("Erro: Não foi possível alocar memória para a fila de processos prontos.");

        // Marca o sistema operacional como com erro interno
        self->erro_interno = true;

        // Libera a memória alocada para o sistema operacional e retorna
        free(self);
        return;
    }

    // Inicializa os ponteiros da fila como NULL (fila vazia)
    self->fila_prontos->inicio = NULL;
    self->fila_prontos->fim = NULL;

    // Mensagem de depuração
    console_printf("Erro: Não foi possível alocar memória para a fila de processos prontos.");
}

static void cpu_inicializa(so_t *self)
{
  // quando a CPU executar uma instrução CHAMAC, deve chamar a função
  //   so_trata_interrupcao, com primeiro argumento um ptr para o SO
  cpu_define_chamaC(self->cpu, so_trata_interrupcao, self);

  // coloca o tratador de interrupção na memória
  // quando a CPU aceita uma interrupção, passa para modo supervisor,
  //   salva seu estado à partir do endereço 0, e desvia para o endereço
  //   IRQ_END_TRATADOR
  // colocamos no endereço IRQ_END_TRATADOR o programa de tratamento
  //   de interrupção (escrito em asm). esse programa deve conter a
  //   instrução CHAMAC, que vai chamar so_trata_interrupcao (como
  //   foi definido acima)
  int ender = so_carrega_programa(self, "trata_int.maq");
  if (ender != IRQ_END_TRATADOR)
  {
    console_printf("SO: problema na carga do programa de tratamento de interrupção");
    self->erro_interno = true;
  }

  // programa o relógio para gerar uma interrupção após INTERVALO_INTERRUPCAO
  if (es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO) != ERR_OK)
  {
    console_printf("SO: problema na programação do timer");
    self->erro_interno = true;
  }
}

/**
 * @brief Cria e inicializa o sistema operacional.
 * 
 * Esta função aloca memória para a estrutura do sistema operacional (`so_t`)
 * e inicializa seus componentes principais, como CPU, memória, dispositivos
 * de entrada/saída e métricas. Caso qualquer etapa falhe, a memória alocada
 * é liberada e `NULL` é retornado.
 * 
 * @param cpu Ponteiro para a CPU do sistema.
 * @param mem Ponteiro para a memória principal do sistema.
 * @param es Ponteiro para os dispositivos de entrada/saída.
 * @param console Ponteiro para o console de depuração.
 * 
 * @return Ponteiro para a estrutura do sistema operacional ou `NULL` se a criação falhar.
 */
so_t *so_cria(cpu_t *cpu, mem_t *mem, es_t *es, console_t *console) {
    // Aloca memória para o sistema operacional
    so_t *self = malloc(sizeof(*self));
    if (self == NULL) {
        console_printf("Erro: Falha ao alocar memória para o sistema operacional.");
        return NULL;
    }

    // Inicializa os componentes básicos do SO
    self->cpu = cpu;
    self->mem = mem;
    self->es = es;
    self->console = console;
    self->erro_interno = false;
    self->processo_corrente = NULL;
    self->pid_atual = 1;
    self->quantum_proc = QUANTUM;
    self->numero_processos = 0;
    self->relogio_atual = -1;

    // Inicializa a fila de processos prontos
    configura_fila_prontos(self);
    if (self->erro_interno) {
        console_printf("Erro: Falha ao configurar a fila de processos prontos.");
        free(self);
        return NULL;
    }

    // Inicializa métricas
    inicializa_metricas(self);

    // Inicializa a CPU
  cpu_inicializa(self);
    if (self->erro_interno) {
    console_printf("Erro: Falha ao inicializar a CPU.");
    free(self->fila_prontos); // Libera a memória da fila de processos prontos
    free(self);               // Libera o próprio objeto do sistema operacional
    return NULL;
  } 

    // Mensagem de sucesso
    console_printf("Info: Sistema operacional criado com sucesso.");

    return self;
}

/**
 * @brief Libera todos os recursos associados ao sistema operacional.
 * 
 * Esta função libera a memória alocada para os componentes do sistema operacional,
 * incluindo processos, a fila de processos prontos e o próprio sistema operacional.
 * Também redefine o tratador de interrupções da CPU para `NULL`.
 * 
 * @param self Ponteiro para o sistema operacional (`so_t`) a ser destruído.
 */
void so_destroi(so_t *self) {
    if (self == NULL) {
        return; // Proteção contra ponteiro nulo
    }

    // Redefine o tratador de interrupções da CPU
    cpu_define_chamaC(self->cpu, NULL, NULL);

    // Libera a memória alocada para os processos
    if (self->processos != NULL) {
        for (int i = 0; i < self->numero_processos; i++) {
            if (self->processos[i] != NULL) {
                free(self->processos[i]);
            }
        }
        free(self->processos);
    }

    // Libera a memória da fila de processos prontos
    if (self->fila_prontos != NULL) {
        no_fila_t *no_atual = self->fila_prontos->inicio;
        while (no_atual != NULL) {
            no_fila_t *no_proximo = no_atual->proximo;
            free(no_atual); // Libera o nó atual
            no_atual = no_proximo;
        }
        free(self->fila_prontos); // Libera a estrutura da fila
    }

    // Libera a memória do sistema operacional
    free(self);

    console_printf("Info: Sistema operacional destruído com sucesso.");
}

// TRATAMENTO DE INTERRUPÇÃO {{{1

// funções auxiliares para o tratamento de interrupção
static void salva_estado_cpu_no_processo(so_t *self);
static void so_trata_irq(so_t *self, int irq);
static void so_trata_pendencias(so_t *self);
static void so_escalona(so_t *self, int escalonador);
static void escalonador_simples(so_t *self);
static void escalonador_round_robin(so_t *self);
static void escalonador_prioridade(so_t *self);
static int so_despacha(so_t *self);

/**
 * @brief Finaliza as operações do sistema operacional.
 * 
 * Esta função realiza as seguintes etapas:
 * - Desativa o timer e o sinalizador de interrupção do sistema.
 * - Salva as métricas finais no arquivo especificado.
 * - Caso algum erro ocorra durante o desligamento do timer ou do sinalizador,
 *   registra uma mensagem de erro e marca o estado interno do sistema como inválido.
 * 
 * @param self Ponteiro para o sistema operacional (`so_t`) a ser finalizado.
 * @return Sempre retorna 1, indicando que a função foi concluída.
 */
static int finaliza_sistema(so_t *self) {
    // Verifica se o ponteiro do sistema operacional é válido
    if (self == NULL) {
        console_printf("Erro: Ponteiro do sistema operacional é nulo.\n");
        return 1; // Retorna imediatamente para evitar comportamento inesperado
    }

    // Desativa o timer
    err_t e1 = es_escreve(self->es, D_RELOGIO_TIMER, 0);

    // Desativa o sinalizador de interrupção
    err_t e2 = es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0);

    // Verifica se ambas as operações foram bem-sucedidas
    if (e1 != ERR_OK || e2 != ERR_OK) {
        console_printf("SO: Não foi possível desligar o timer ou o sinalizador de interrupção!\n");
        self->erro_interno = true; // Marca erro interno
    } else {
        console_printf("SO: Timer e sinalizador de interrupção desativados com sucesso.\n");
    }

    // Salva as métricas finais no arquivo especificado
    so_salva_metricas(self, "metricas_final.txt");
    console_printf("SO: Métricas finais salvas no arquivo 'metricas_final.txt'.\n");

    // Retorna 1 indicando que a função foi concluída
    return 1;
}

/**
 * @brief Atualiza as métricas do sistema operacional com base no relógio do sistema.
 * 
 * Essa função lê o valor atual do relógio de instruções, calcula a diferença
 * de tempo em relação ao valor anterior, e atualiza as métricas globais do sistema
 * com base nessa diferença. Caso o valor anterior seja inválido (inicializado como -1),
 * a função retorna sem realizar cálculos.
 * 
 * @param self Ponteiro para o sistema operacional (`so_t`).
 */
static void atualiza_metricas_com_relogio(so_t *self) {
    // Armazena o valor anterior do relógio
    int r_anterior = self->relogio_atual;

    // Lê o valor atual do relógio de instruções
    if (es_le(self->es, D_RELOGIO_INSTRUCOES, &self->relogio_atual) != ERR_OK) {
        // Imprime uma mensagem de erro se a leitura falhar
        console_printf("SO: erro na leitura do relógio\n");
        return;
    }

    // Verifica se o valor anterior é inválido (ex.: primeiro ciclo de execução)
    if (r_anterior == -1) {
        return; // Nenhuma métrica pode ser calculada neste caso
    }

    // Calcula a diferença de tempo entre o valor atual e o anterior
    int dif_tempo = self->relogio_atual - r_anterior;

    // Atualiza as métricas do sistema com base na diferença de tempo
    atualiza_metricas_sistema(self, dif_tempo);
}

// função a ser chamada pela CPU quando executa a instrução CHAMAC, no tratador de
//   interrupção em assembly
// essa é a única forma de entrada no SO depois da inicialização
// na inicialização do SO, a CPU foi programada para chamar esta função para executar
//   a instrução CHAMAC
// a instrução CHAMAC só deve ser executada pelo tratador de interrupção
//
// o primeiro argumento é um ponteiro para o SO, o segundo é a identificação
//   da interrupção
// o valor retornado por esta função é colocado no registrador A, e pode ser
//   testado pelo código que está após o CHAMAC. No tratador de interrupção em
//   assembly esse valor é usado para decidir se a CPU deve retornar da interrupção
//   (e executar o código de usuário) ou executar PARA e ficar suspensa até receber
//   outra interrupção
static int so_trata_interrupcao(void *argC, int reg_A) {
    so_t *self = argC;
    irq_t irq = reg_A;

    // Incrementa a contagem de interrupções do tipo atual
    self->metricas.num_interrupcoes[irq]++;

    // salva o estado da cpu no descritor do processo que foi interrompido
    salva_estado_cpu_no_processo(self);

    // Atualiza métricas do sistema
    atualiza_metricas_com_relogio(self);

    // Trata a interrupção com base no tipo de IRQ
    so_trata_irq(self, irq);

    // Processa pendências independentes da interrupção
    so_trata_pendencias(self);

    // Escolhe o próximo processo a executar
    so_escalona(self, ESCALONADOR);

    // Verifica se ainda há processos ativos
    bool processos_ativos = false;
    for (int i = 0; i < self->numero_processos; i++) {
        if (self->processos[i]->estado != ESTADO_TERMINADO) {
            processos_ativos = true;
            break; // Encontrou um processo ativo, pode parar a verificação
        }
    }

    // Executa ou finaliza o sistema com base na verificação
    if (processos_ativos) {
        return so_despacha(self); // Continua executando
    } else {
        return finaliza_sistema(self); // Finaliza o sistema
    }
}

/**
 * @brief Salva o estado da CPU no processo corrente.
 * 
 * Esta função armazena o estado atual da CPU no descritor do processo
 * corrente. Isso inclui:
 * - O valor do PC (contador de programa), indicando onde o programa será retomado.
 * - Os valores dos registradores de propósito geral (A e X).
 * 
 * Se não houver um processo corrente, a função não realiza nenhuma operação.
 * 
 * @param self Ponteiro para o sistema operacional (`so_t`).
 */
static void salva_estado_cpu_no_processo(so_t *self) {
    // Verifica se há um processo corrente
    if (self->processo_corrente == NULL) {
        return; // Nenhuma operação necessária
    }

    // Lê e armazena o valor do PC (program counter) no processo corrente
    if (mem_le(self->mem, IRQ_END_PC, &self->processo_corrente->pc) != ERR_OK) {
        console_printf("SO: erro ao salvar o PC no processo corrente.\n");
    }

    // Lê e armazena os registradores de propósito geral no processo corrente
    if (mem_le(self->mem, IRQ_END_A, &self->processo_corrente->reg[0]) != ERR_OK) {
        console_printf("SO: erro ao salvar o registrador A no processo corrente.\n");
    }
    if (mem_le(self->mem, IRQ_END_X, &self->processo_corrente->reg[1]) != ERR_OK) {
        console_printf("SO: erro ao salvar o registrador X no processo corrente.\n");
    }
}

static int calcular_endereco_dispositivo(int disp, int terminal)
{
    // Multiplicador usado para separar os dispositivos de diferentes terminais
    const int MULTIPLICADOR_TERMINAL = 4;

    // Calcula e retorna o identificador único do dispositivo
    return disp + terminal * MULTIPLICADOR_TERMINAL;
}

static void insere_na_fila_prontos(so_t *self, processo_t *proc)
{
  no_fila_t *novo_no = malloc(sizeof(no_fila_t));
  if (novo_no == NULL)
  {
    console_printf("SO: erro ao alocar memória para a fila de processos prontos");
    self->erro_interno = true;
    return;
  }
  novo_no->processo = proc;
  novo_no->proximo = NULL;

  if (self->fila_prontos->fim == NULL)
  {
    self->fila_prontos->inicio = novo_no;
  }
  else
  {
    self->fila_prontos->fim->proximo = novo_no;
  }
  self->fila_prontos->fim = novo_no;
}

static void so_trata_pendencias(so_t *self)
{
  for (int i = 0; self->processos[i] != NULL; i++)
  {
    processo_t *proc = self->processos[i];

    if (proc->estado == ESTADO_BLOQUEADO)
    {
      int terminal = obter_terminal_por_pid(proc->pid);
      int dispositivo_teclado = calcular_endereco_dispositivo(D_TERM_A_TECLADO_OK, terminal);
      int dispositivo_tela = calcular_endereco_dispositivo(D_TERM_A_TELA_OK, terminal);

      int estado_teclado, estado_tela;

      switch (proc->motivo_bloqueio)
      {
      case BLOQUEIO_POR_LEITURA:
        // se o dispositivo de teclado está pronto, le
        if (es_le(self->es, dispositivo_teclado, &estado_teclado) == ERR_OK && estado_teclado != 0)
        {
          proc_muda_estado(proc, ESTADO_PRONTO);
        }
        break;
      case BLOQUEIO_POR_ESCRITA:
        // se o dispositivo de tela está pronto, escreve
        if (es_le(self->es, dispositivo_tela, &estado_tela) == ERR_OK && estado_tela != 0)
        {
          if (es_escreve(self->es, calcular_endereco_dispositivo(D_TERM_A_TELA, terminal), proc->dado_pendente) == ERR_OK)
          {
            proc_muda_estado(proc, ESTADO_PRONTO);
          }
        }
        break;
      case BLOQUEIO_POR_ESPERA_DE_PROC:
        // verifica se o processo esperado já morreu
        for (int j = 0; self->processos[j] != NULL; j++)
        {
          if (self->processos[j]->pid == proc->reg[0] && self->processos[j]->estado == ESTADO_TERMINADO)
          {
            proc_muda_estado(proc, ESTADO_PRONTO);
            break;
          }
        }
        break;
      default:
        break;
      }

      if (proc->estado == ESTADO_PRONTO)
      {
        insere_na_fila_prontos(self, proc);
        console_printf("SO: processo %d desbloqueado e inserido na fila de prontos", proc->pid);
      }
    }
  }
}
static void atualiza_estado_processo_corrente(so_t *self)
{
  if (self->processo_corrente != NULL)
  {
    console_printf("SO: escalonando, processo corrente %d, estado %s", self->processo_corrente->pid, estado_processo_para_string(self->processo_corrente->estado));
    if (self->processo_corrente->estado == ESTADO_PRONTO)
      self->processo_corrente->estado = ESTADO_INICIALIZANDO;
  }
}

static void atualiza_prioridade(so_t *self)
{
  if (self->processo_corrente != NULL)
  {
    self->processo_corrente->prioridade = self->processo_corrente->prioridade + (QUANTUM - self->quantum_proc) / (float)QUANTUM / 2;
  }
}

static void so_escalona(so_t *self, int escalonador)
{

  atualiza_estado_processo_corrente(self);
  atualiza_prioridade(self);

  switch (escalonador)
  {
  case 1:
    escalonador_prioridade(self);
    break;
  case 2:
    escalonador_round_robin(self);
    break;
  case 3:
    escalonador_simples(self);
    break;
    self->erro_interno = true;
  }
  if (self->processo_corrente != NULL)
    console_printf("SO: escalonado, processo corrente %d, estado %s", self->processo_corrente->pid, estado_processo_para_string(self->processo_corrente->estado));
}

static void so_executa_proc(so_t *self, processo_t *proc)
{
  if (self->processo_corrente != NULL && proc != NULL)
    console_printf("--SO: processo %d, estado %s, processo_so %d, estado %s", proc->pid, estado_processo_para_string(proc->estado), self->processo_corrente->pid, estado_processo_para_string(self->processo_corrente->estado));

  if (
      self->processo_corrente != NULL &&
      self->processo_corrente != proc &&
      self->processo_corrente->estado == ESTADO_INICIALIZANDO)
  {
    proc_muda_estado(self->processo_corrente, ESTADO_PRONTO);
    self->metricas.num_preempcoes++;
    console_printf("SO: processo %d preempedido", self->processo_corrente->pid);
  }

  if (proc != NULL && proc->estado != ESTADO_INICIALIZANDO)
  {
    console_printf("SO: processo %d executando", proc->pid);
    proc_muda_estado(proc, ESTADO_INICIALIZANDO);
  }

  self->processo_corrente = proc;
  self->quantum_proc = QUANTUM;
}

/**
 * @brief Procura por um processo em um estado específico na lista de processos.
 * 
 * Essa função percorre a lista de processos e retorna o primeiro processo
 * que está no estado especificado. Caso nenhum processo esteja no estado buscado,
 * retorna `NULL`.
 * 
 * @param self Ponteiro para o sistema operacional (`so_t`).
 * @param estado O estado que estamos procurando (ex.: ESTADO_PRONTO, ESTADO_BLOQUEADO).
 * 
 * @return Ponteiro para o processo encontrado no estado especificado ou `NULL` se nenhum processo for encontrado.
 */
static processo_t *obtem_processo_por_estado(so_t *self, estado_processo_t estado) {
    // Percorre a lista de processos no SO
    for (int i = 0; self->processos[i] != NULL; i++) {
        // Verifica se o processo atual está no estado desejado
        if (self->processos[i]->estado == estado) {
            // Retorna o processo encontrado
            return self->processos[i];
        }
    }
    // Se nenhum processo no estado especificado foi encontrado, retorna NULL
    return NULL;
}

/**
 * @brief Escalonador simples.
 *
 * Este escalonador utiliza uma abordagem simples: seleciona o próximo processo
 * pronto para execução e o escalona. Caso não haja processos prontos, verifica se
 * há processos bloqueados. Se nenhum processo estiver disponível, o sistema operacional
 * é configurado para parar, sinalizando que todos os processos finalizaram.
 *
 * @param sistema_operacional Ponteiro para o sistema operacional (SO).
 */
static void escalonador_simples(so_t *sistema_operacional) {
    // Verifica se o processo corrente está executando
    if (sistema_operacional->processo_corrente != NULL &&
        sistema_operacional->processo_corrente->estado == ESTADO_INICIALIZANDO) {
        return; // Processo atual continua executando
    }

    // Obtém o próximo processo pronto para execução
    processo_t *proximo_processo = obtem_processo_por_estado(sistema_operacional, ESTADO_PRONTO);
    if (proximo_processo != NULL) {
        so_executa_proc(sistema_operacional, proximo_processo);
        return; // Processo pronto encontrado e escalado
    }

    // Verifica se há processos bloqueados
    if (obtem_processo_por_estado(sistema_operacional, ESTADO_BLOQUEADO)) {
        sistema_operacional->processo_corrente = NULL; // Nenhum processo pronto, mantém estado atual
    } else {
        // Nenhum processo restante, o sistema operacional será finalizado
        console_printf("SO: todos os processos finalizaram, CPU parando");
        sistema_operacional->erro_interno = true;
    }
}

static processo_t *remove_primeiro_processo_fila(fila_t *fila)
{
  if (fila->inicio == NULL)
  {
    return NULL;
  }

  no_fila_t *no = fila->inicio;
  processo_t *proc = no->processo;
  fila->inicio = no->proximo;
  if (fila->inicio == NULL)
  {
    fila->fim = NULL;
  }
  free(no);
  return proc;
}

/**
 * @brief Escalonador baseado em Round-Robin.
 *
 * Implementa a lógica de escalonamento Round-Robin, onde cada processo recebe
 * um quantum de tempo para execução. Se o processo corrente ainda possui quantum,
 * ele continua executando. Caso contrário, é movido para o final da fila de prontos,
 * e o próximo processo na fila é escalado.
 *
 * @param sistema_operacional Ponteiro para o sistema operacional (SO).
 */
static void escalonador_round_robin(so_t *sistema_operacional) {
    // Verifica se o processo corrente está executando e ainda possui quantum restante
    if (sistema_operacional->processo_corrente != NULL &&
        sistema_operacional->processo_corrente->estado == ESTADO_INICIALIZANDO &&
        sistema_operacional->quantum_proc > 0) {
        return; // Processo atual continua executando
    }

    // Se o processo corrente esgotou seu quantum, move-o para o final da fila de prontos
    if (sistema_operacional->processo_corrente != NULL &&
        sistema_operacional->processo_corrente->estado == ESTADO_INICIALIZANDO) {
        insere_na_fila_prontos(sistema_operacional, sistema_operacional->processo_corrente);
    }

    // Verifica se há processos na fila de prontos
    if (sistema_operacional->fila_prontos->inicio != NULL) {
        // Remove o próximo processo da fila e o escalona para execução
        processo_t *proximo_processo = remove_primeiro_processo_fila(sistema_operacional->fila_prontos);
        so_executa_proc(sistema_operacional, proximo_processo);
    } else {
        // Se não há processos prontos, define o processo corrente como NULL
        sistema_operacional->processo_corrente = NULL;
    }
}

/**
 * @brief Busca e remove o nó com o processo de maior prioridade na fila.
 * 
 * Essa função percorre a fila de processos prontos para encontrar o nó
 * contendo o processo com a maior prioridade (menor valor numérico).
 * Após encontrar, remove o nó da fila e ajusta os ponteiros de início e fim.
 * 
 * @param fila Ponteiro para a estrutura da fila.
 * @return Ponteiro para o nó contendo o processo de maior prioridade.
 */
static no_fila_t *remover_processo_maior_prioridade(fila_t *fila) {
    if (fila->inicio == NULL) {
        return NULL; // Fila vazia
    }

    no_fila_t *no_anterior = NULL;
    no_fila_t *no_maior_prioridade = fila->inicio;
    no_fila_t *no_maior_prioridade_anterior = NULL;
    no_fila_t *no_atual = fila->inicio;

    // Percorre a fila para encontrar o nó com maior prioridade
    while (no_atual != NULL) {
        if (no_atual->processo->prioridade < no_maior_prioridade->processo->prioridade) {
            no_maior_prioridade = no_atual;
            no_maior_prioridade_anterior = no_anterior;
        }
        no_anterior = no_atual;
        no_atual = no_atual->proximo;
    }

    // Remove o nó de maior prioridade da fila
    if (no_maior_prioridade_anterior != NULL) {
        no_maior_prioridade_anterior->proximo = no_maior_prioridade->proximo;
    } else {
        fila->inicio = no_maior_prioridade->proximo;
    }

    // Ajusta o ponteiro do fim da fila, se necessário
    if (no_maior_prioridade == fila->fim) {
        fila->fim = no_maior_prioridade_anterior;
    }

    return no_maior_prioridade;
}

/**
 * @brief Escalonador baseado em prioridade.
 *
 * Essa função implementa a lógica de escalonamento que seleciona o processo com a maior prioridade
 * (valor numérico mais baixo) da fila de processos prontos. Antes de escalar um novo processo,
 * verifica o estado do processo corrente, gerencia o quantum de execução e o insere novamente
 * na fila de prontos, se necessário.
 *
 * @param self Ponteiro para o sistema operacional (SO).
 */
static void escalonador_prioridade(so_t *sistema_operacional) {
    // Verifica se há um processo corrente e imprime detalhes
    if (sistema_operacional->processo_corrente != NULL) {
        console_printf(
            "Processo Corrente: %d, Estado: %s",
            sistema_operacional->processo_corrente->pid,
            estado_processo_para_string(sistema_operacional->processo_corrente->estado)
        );
    }

    // Se o processo corrente está em execução e ainda possui quantum restante, mantém a execução
    if (sistema_operacional->processo_corrente != NULL &&
        sistema_operacional->processo_corrente->estado == ESTADO_INICIALIZANDO &&
        sistema_operacional->quantum_proc > 0) {
        return;
    }

    // Se o processo corrente terminou seu quantum, insere-o de volta na fila de prontos
    if (sistema_operacional->processo_corrente != NULL &&
        sistema_operacional->processo_corrente->estado == ESTADO_INICIALIZANDO) {
        insere_na_fila_prontos(sistema_operacional, sistema_operacional->processo_corrente);
    }

    // Verifica se há processos prontos na fila
    if (sistema_operacional->fila_prontos->inicio != NULL) {
        // Obtém o nó do processo com maior prioridade
        no_fila_t *nodo_maior_prioridade = remover_processo_maior_prioridade(sistema_operacional->fila_prontos);

        // Imprime informações sobre o processo a ser escalado
        if (sistema_operacional->processo_corrente != NULL && nodo_maior_prioridade->processo != NULL) {
            console_printf(
                "SO: Escalonando processo de maior prioridade, PID: %d, Estado: %s",
                nodo_maior_prioridade->processo->pid,
                estado_processo_para_string(nodo_maior_prioridade->processo->estado)
            );
            console_printf(
                "SO: Processo anterior, PID: %d, Estado: %s",
                sistema_operacional->processo_corrente->pid,
                estado_processo_para_string(sistema_operacional->processo_corrente->estado)
            );
        }

        // Executa o processo de maior prioridade
        so_executa_proc(sistema_operacional, nodo_maior_prioridade->processo);

        // Libera o nó da fila (somente estrutura, não o processo)
        free(nodo_maior_prioridade);
    } else {
        // Se não há processos na fila, define o processo corrente como NULL
        sistema_operacional->processo_corrente = NULL;
    }
}

static int so_despacha(so_t *self)
{
  if (self->processo_corrente == NULL)
  {
    return 1;
  }

  mem_escreve(self->mem, IRQ_END_PC, self->processo_corrente->pc);
  mem_escreve(self->mem, IRQ_END_A, self->processo_corrente->reg[0]);
  mem_escreve(self->mem, IRQ_END_X, self->processo_corrente->reg[1]);

  console_printf("SO: despachando processo %d", self->processo_corrente->pid);

  return 0;
}

// TRATAMENTO DE UMA IRQ {{{1

// funções auxiliares para tratar cada tipo de interrupção
static void so_trata_irq_reset(so_t *self);
static void so_trata_irq_chamada_sistema(so_t *self);
static void so_trata_irq_err_cpu(so_t *self);
static void so_trata_irq_relogio(so_t *self);
static void so_trata_irq_desconhecida(so_t *self, int irq);

static void so_trata_irq(so_t *self, int irq)
{
  // verifica o tipo de interrupção que está acontecendo, e atende de acordo
  switch (irq)
  {
  case IRQ_RESET:
    so_trata_irq_reset(self);
    break;
  case IRQ_SISTEMA:
    so_trata_irq_chamada_sistema(self);
    break;
  case IRQ_ERR_CPU:
    so_trata_irq_err_cpu(self);
    break;
  case IRQ_RELOGIO:
    so_trata_irq_relogio(self);
    break;
  default:
    so_trata_irq_desconhecida(self, irq);
  }
}

static processo_t *so_cria_processo(so_t *self, char *nome_do_executavel)
{
  processo_t *proc = aloca_processo();
  if (proc == NULL)
  {
    return NULL;
  }

  int pc = so_carrega_programa(self, nome_do_executavel);
  console_printf("SO: processo %d criado com PC=%d", self->pid_atual, pc);
  if (pc == -1)
  {
    console_printf("SO: erro ao carregar o programa '%s'", nome_do_executavel);
    free(proc);
    return NULL;
  }

  inicializa_processo(proc, self->pid_atual++, pc);
  self->numero_processos++;

  return proc;
}

// interrupção gerada uma única vez, quando a CPU inicializa
static void so_trata_irq_reset(so_t *self)
{
  // cria um processo para o init
  processo_t *init_proc = so_cria_processo(self, "init.maq");
  if (init_proc == NULL)
  {
    console_printf("SO: problema na carga do programa inicial");
    self->erro_interno = true;
    return;
  }

  // adiciona o processo init à lista de processos
  self->processos = malloc(sizeof(processo_t *));
  if (self->processos == NULL)
  {
    console_printf("SO: problema na criação da lista de processos");
    self->erro_interno = true;
    free(init_proc);
    return;
  }
  self->processos[0] = init_proc;
  self->processo_corrente = init_proc;

  // altera o PC para o endereço de carga
  mem_escreve(self->mem, IRQ_END_PC, init_proc->pc);
  // passa o processador para modo usuário
  // mem_escreve(self->mem, IRQ_END_modo, usuario);
}

/**
 * @brief Verifica e desbloqueia processos esperando pela morte de outro processo.
 *
 * Esta função percorre a lista de processos para identificar aqueles que estão bloqueados
 * aguardando a finalização de um processo específico (identificado por `pid_morto`).
 * Ao encontrar tais processos, altera seu estado para "PRONTO" e registra a ação no log.
 *
 * @param sistema_operacional Ponteiro para o sistema operacional.
 * @param pid_morto Identificador do processo que foi finalizado.
 */
static void verifica_processos_em_espera(so_t *sistema_operacional, int pid_morto) {
    for (int i = 0; sistema_operacional->processos[i] != NULL; i++) {
        processo_t *processo_atual = sistema_operacional->processos[i];

        // Verifica se o processo está bloqueado aguardando
        if (processo_atual->estado == ESTADO_BLOQUEADO) {
            // Altera o estado do processo para PRONTO
            processo_atual->estado = ESTADO_PRONTO;

            // Log informativo
            console_printf(
                "[INFO] Processo desbloqueado: PID=%d. Motivo: término do processo PID=%d.\n",
                processo_atual->pid,
                pid_morto
            );
        }
    }
}

// interrupção gerada quando a CPU identifica um erro
static void so_trata_irq_err_cpu(so_t *self)
{
  int err_int;
  mem_le(self->mem, IRQ_END_erro, &err_int);
  err_t err = err_int;
  console_printf("SO: erro na CPU: %s", err_nome(err));

  if (self->processo_corrente != NULL)
  {
    console_printf("SO: matando processo %d devido a erro na CPU", self->processo_corrente->pid);
    self->processo_corrente->estado = BLOQUEIO_POR_LEITURA;
    verifica_processos_em_espera(self, self->processo_corrente->pid);
    self->processo_corrente = NULL;
  }
  else
  {
    console_printf("SO: erro na CPU sem processo corrente");
  }

  self->erro_interno = true;
}

// interrupção gerada quando o timer expira
static void so_trata_irq_relogio(so_t *self)
{
  // rearma o interruptor do relógio e reinicializa o timer para a próxima interrupção
  err_t e1, e2;
  e1 = es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0); // desliga o sinalizador de interrupção
  e2 = es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO);
  if (e1 != ERR_OK || e2 != ERR_OK)
  {
    console_printf("SO: problema da reinicialização do timer");
    self->erro_interno = true;
  }
  // decrementando o quantum
  if (self->quantum_proc > 0)
  {
    self->quantum_proc--;
  }
  console_printf("Quantum: %d", self->quantum_proc);
}

// foi gerada uma interrupção para a qual o SO não está preparado
static void so_trata_irq_desconhecida(so_t *self, int irq)
{
  console_printf("SO: não sei tratar IRQ %d (%s)", irq, irq_nome(irq));
  self->erro_interno = true;
}

// funções auxiliares para cada chamada de sistema
static void so_chamada_le(so_t *self);
static void so_chamada_escr(so_t *self);
static void so_chamada_cria_proc(so_t *self);
static void so_chamada_mata_proc(so_t *self);
static void so_chamada_espera_proc(so_t *self);

static void so_trata_irq_chamada_sistema(so_t *self)
{
  // a identificação da chamada está no registrador A
  // t1: com processos, o reg A tá no descritor do processo corrente
  int id_chamada;
  if (mem_le(self->mem, IRQ_END_A, &id_chamada) != ERR_OK)
  {
    console_printf("SO: erro no acesso ao id da chamada de sistema");
    self->erro_interno = true;
    return;
  }
  console_printf("SO: chamada de sistema %d", id_chamada);
  switch (id_chamada)
  {
  case SO_LE:
    so_chamada_le(self);
    break;
  case SO_ESCR:
    so_chamada_escr(self);
    break;
  case SO_CRIA_PROC:
    so_chamada_cria_proc(self);
    break;
  case SO_MATA_PROC:
    so_chamada_mata_proc(self);
    break;
  case SO_ESPERA_PROC:
    so_chamada_espera_proc(self);

    break;
  default:
    console_printf("SO: chamada de sistema desconhecida (%d)", id_chamada);
    // t1: deveria matar o processo
    so_chamada_mata_proc(self);
  }
}

// implementação da chamada se sistema SO_LE
// faz a leitura de um dado da entrada corrente do processo, coloca o dado no reg A
/**
 * @brief Obtém o terminal correspondente a um PID.
 *
 * Esta função calcula o terminal associado a um processo com base em seu PID.
 * O cálculo usa uma abordagem modular para distribuir os processos de forma
 * uniforme entre os terminais disponíveis.
 *
 * @param pid Identificador único do processo.
 * @return O índice do terminal correspondente (0 a NUM_TERMINAIS - 1).
 */
static int obter_terminal_por_pid(int pid) {
    const int NUM_TERMINAIS = 4; // Número total de terminais disponíveis

    // Verifica se o PID é válido
    if (pid <= 0) {
        console_printf("[ERRO] PID inválido: %d. Deve ser maior que zero.\n", pid);
        return -1; // Retorna -1 para indicar erro
    }

    // Calcula o terminal correspondente
    int terminal = (pid - 1) % NUM_TERMINAIS;

    // Log de depuração (opcional)
    console_printf("[INFO] PID=%d associado ao terminal=%d.\n", pid, terminal);

    return terminal;
}

static void so_chamada_le(so_t *self)
{
  int terminal = obter_terminal_por_pid(self->processo_corrente->pid);
  int dispositivo = calcular_endereco_dispositivo(D_TERM_A_TECLADO, terminal);
  int dispositivo_ok = calcular_endereco_dispositivo(D_TERM_A_TECLADO_OK, terminal);

  int estado;
  if (es_le(self->es, dispositivo_ok, &estado) != ERR_OK)
  {
    console_printf("SO: problema no acesso ao estado do teclado");
    self->erro_interno = true;
    return;
  }

  if (estado == 0)
  {
    // dispositivo ocupado, bloquear o processo
    proc_muda_estado(self->processo_corrente, ESTADO_BLOQUEADO);
    self->processo_corrente->motivo_bloqueio = BLOQUEIO_POR_LEITURA; // definindo motivo do bloqueio
    return;
  }

  int dado;
  if (es_le(self->es, dispositivo, &dado) != ERR_OK)
  {
    console_printf("SO: problema no acesso ao teclado do terminal %d", terminal);
    self->erro_interno = true;
    return;
  }

  mem_escreve(self->mem, IRQ_END_A, dado);
}
/// implementação da chamada se sistema SO_ESCR
// escreve o valor do reg X na saída corrente do processo
static void so_chamada_escr(so_t *self)
{

  int terminal = obter_terminal_por_pid(self->processo_corrente->pid);

  int dispositivo_tela = calcular_endereco_dispositivo(D_TERM_A_TELA, terminal);
  int dispositivo_tela_ok = calcular_endereco_dispositivo(D_TERM_A_TELA_OK, terminal);

  // verifica o estado do dispositivo de tela
  int estado;
  if (es_le(self->es, dispositivo_tela_ok, &estado) != ERR_OK)
  {
    console_printf("SO: problema no acesso ao estado da tela do terminal %d", terminal);
    self->erro_interno = true;
    return;
  }

  // se o dispositivo está ocupado, salva o dado pendente e bloqueia o processo
  if (estado == 0)
  {
    if (mem_le(self->mem, IRQ_END_X, &self->processo_corrente->dado_pendente) != ERR_OK)
    {
      console_printf("SO: problema ao ler o valor do registrador X");
      self->erro_interno = true;
      return;
    }
    proc_muda_estado(self->processo_corrente, ESTADO_BLOQUEADO);
    self->processo_corrente->motivo_bloqueio = BLOQUEIO_POR_ESCRITA; // definindo motivo do bloqueio
    return;
  }

  // lê o valor do registrador X
  int dado;
  if (mem_le(self->mem, IRQ_END_X, &dado) != ERR_OK)
  {
    self->erro_interno = true;
    return;
  }

  if (es_escreve(self->es, dispositivo_tela, dado) != ERR_OK)
  {
    return;
  }

  mem_escreve(self->mem, IRQ_END_A, 0);
}

/**
 * @brief Adiciona um novo processo à lista de processos do SO.
 *
 * Esta função insere um processo na lista dinâmica de processos do sistema
 * operacional, realocando memória conforme necessário. Caso ocorra um erro
 * durante a realocação, o processo não é adicionado e a memória alocada para
 * ele é liberada.
 *
 * @param self Ponteiro para o sistema operacional.
 * @param novo_proc Ponteiro para o processo a ser adicionado.
 */
static void adiciona_processo_na_lista(so_t *self, processo_t *novo_proc) {
    // Validação inicial de argumentos
    if (self == NULL || novo_proc == NULL) {
        console_printf("[ERRO] Argumento inválido em adiciona_processo_na_lista.\n");
        return;
    }

    // Verifica se a lista de processos está inicializada
    if (self->processos == NULL) {
        console_printf("[INFO] Inicializando a lista de processos.\n");

        // Aloca espaço inicial para dois ponteiros (um para o processo e outro para o terminador NULL)
        self->processos = malloc(2 * sizeof(processo_t *));
        if (self->processos == NULL) {
            console_printf("[ERRO] Falha ao inicializar a lista de processos.\n");
            free(novo_proc);
            return;
        }

        // Inicializa a lista com o novo processo e terminador NULL
        self->processos[0] = novo_proc;
        self->processos[1] = NULL;
        return;
    }

    // Calcula o número atual de processos
    int i = 0;
    while (self->processos[i] != NULL) {
        i++;
    }

    // Realoca a lista para adicionar mais um processo
    processo_t **nova_lista = realloc(self->processos, (i + 2) * sizeof(processo_t *));
    if (nova_lista == NULL) {
        console_printf("[ERRO] Falha ao realocar a lista de processos.\n");
        free(novo_proc); // Libera o processo não adicionado
        return;
    }

    // Atualiza a lista e adiciona o novo processo
    self->processos = nova_lista;
    self->processos[i] = novo_proc;
    self->processos[i + 1] = NULL;

    console_printf("[INFO] Processo PID=%d adicionado com sucesso.\n", novo_proc->pid);
}


// implementação da chamada se sistema SO_CRIA_PROC
// cria um processo
static void so_chamada_cria_proc(so_t *self)
{
  int ender_nome;
  if (mem_le(self->mem, IRQ_END_X, &ender_nome) != ERR_OK)
  {
    console_printf("SO: erro ao acessar o endereço do nome do arquivo");
    self->erro_interno = true;
    mem_escreve(self->mem, IRQ_END_A, -1);
    return;
  }

  // copia o nome do arquivo da memória do processo
  char nome[100];
  if (!copia_str_da_mem(100, nome, self->mem, ender_nome))
  {
    console_printf("SO: erro ao copiar o nome do arquivo da memória");
    mem_escreve(self->mem, IRQ_END_A, -1);
    return;
  }

  // cria o novo processo
  processo_t *novo_proc = so_cria_processo(self, nome);

  if (novo_proc == NULL)
  {
    console_printf("SO: erro ao criar o novo processo");
    mem_escreve(self->mem, IRQ_END_A, -1);
    return;
  }

  // adiciona o novo processo à lista de processos
  adiciona_processo_na_lista(self, novo_proc);

  // adiciona o novo processo à fila de prontos
  insere_na_fila_prontos(self, novo_proc);

  int i = 0;
  while (self->processos[i] != NULL)
  {
    console_printf("Lista de processos: %d", self->processos[i]->pid);
    i++;
  }

  self->processo_corrente->reg[0] = novo_proc->pid;
}
static void remove_processo_da_lista(so_t *self, int pid)
{
  no_fila_t *anterior = NULL;
  no_fila_t *atual = self->fila_prontos->inicio;
  while (atual != NULL)
  {
    if (atual->processo->pid == pid)
    {
      if (anterior == NULL)
      {
        self->fila_prontos->inicio = atual->proximo;
      }
      else
      {
        anterior->proximo = atual->proximo;
      }
      if (atual == self->fila_prontos->fim)
      {
        self->fila_prontos->fim = anterior;
      }
      free(atual);
      break;
    }
    anterior = atual;
    atual = atual->proximo;
  }
}

// implementação da chamada se sistema SO_MATA_PROC
// mata o processo com pid X (ou o processo corrente se X é 0)
static void so_chamada_mata_proc(so_t *self)
{
  int pid = self->processo_corrente->reg[1];

  console_printf("SO: matando processo com PID %d", pid);

  if (pid == 0)
  {
    pid = self->processo_corrente->pid;
  }

  for (int i = 0; self->processos[i] != NULL; i++)
  {
    if (self->processos[i]->pid == pid)
    {
      proc_muda_estado(self->processos[i], ESTADO_TERMINADO);
      if (self->processo_corrente->pid == pid)
      {
        self->processo_corrente = NULL;
      }
      mem_escreve(self->mem, IRQ_END_A, 0);

      // remove processo da fila de prontos
      remove_processo_da_lista(self, pid);

      return;
    }
  }

  mem_escreve(self->mem, IRQ_END_A, -1);
}

static processo_t *encontra_processo_por_pid(so_t *self, int pid)
{
  for (int i = 0; self->processos[i] != NULL; i++)
  {
    if (self->processos[i]->pid == pid)
    {
      return self->processos[i];
    }
  }
  return NULL;
}

// implementação da chamada se sistema SO_ESPERA_PROC
// espera o fim do processo com pid X
/**
 * @brief Trata a chamada de sistema para esperar um processo terminar.
 *
 * Essa função verifica se o processo corrente pode esperar pelo término
 * de outro processo. Se o processo a ser esperado não for encontrado, já
 * estiver terminado ou for inválido, a função retorna com um código de erro.
 * Caso contrário, bloqueia o processo corrente até que o processo esperado termine.
 *
 * @param self Ponteiro para o sistema operacional.
 */
static void so_chamada_espera_proc(so_t *self) {
    // Obtém o PID do processo que o processo corrente deseja esperar
    int pid = self->processo_corrente->reg[1];

    // Verifica se o processo corrente está tentando esperar por si mesmo
    if (pid == self->processo_corrente->pid) {
        console_printf("[ERRO] Processo PID=%d não pode esperar por si mesmo.\n", pid);
        mem_escreve(self->mem, IRQ_END_A, -1);
        return;
    }

    // Busca o processo pelo PID
    processo_t *proc_esperado = encontra_processo_por_pid(self, pid);

    // Verifica se o processo foi encontrado
    if (proc_esperado == NULL) {
        console_printf("[ERRO] Processo esperado com PID=%d não encontrado.\n", pid);
        mem_escreve(self->mem, IRQ_END_A, -1);
        return;
    }

    // Verifica se o processo já está terminado
    if (proc_esperado->estado == ESTADO_TERMINADO) {
        console_printf("[INFO] Processo PID=%d já terminou. Nenhuma espera necessária.\n", pid);
        mem_escreve(self->mem, IRQ_END_A, 0);
        return;
    }

    // Bloqueia o processo corrente e define o motivo do bloqueio
    proc_muda_estado(self->processo_corrente, ESTADO_BLOQUEADO);
    self->processo_corrente->motivo_bloqueio = BLOQUEIO_POR_ESPERA_DE_PROC;

    // Armazena o PID do processo que está sendo esperado
    self->processo_corrente->reg[0] = pid;

    // Retorna sucesso ao processo chamador
    console_printf("[INFO] Processo PID=%d agora está aguardando o término do processo PID=%d.\n",
                   self->processo_corrente->pid, pid);
    mem_escreve(self->mem, IRQ_END_A, 0);
}

// CARGA DE PROGRAMA {{{1

// carrega o programa na memória
// retorna o endereço de carga ou -1
static int so_carrega_programa(so_t *self, char *nome_do_executavel)
{
  // programa para executar na nossa CPU
  programa_t *prog = prog_cria(nome_do_executavel);
  if (prog == NULL)
  {
    console_printf("Erro na leitura do programa '%s'\n", nome_do_executavel);
    return -1;
  }

  int end_ini = prog_end_carga(prog);
  int end_fim = end_ini + prog_tamanho(prog);

  for (int end = end_ini; end < end_fim; end++)
  {
    if (mem_escreve(self->mem, end, prog_dado(prog, end)) != ERR_OK)
    {
      console_printf("Erro na carga da memória, endereco %d\n", end);
      return -1;
    }
  }

  prog_destroi(prog);
  console_printf("SO: carga de '%s' em %d-%d", nome_do_executavel, end_ini, end_fim);
  return end_ini;
}

// ACESSO À MEMÓRIA DOS PROCESSOS {{{1

// copia uma string da memória do simulador para o vetor str.
// retorna false se erro (string maior que vetor, valor não char na memória,
//   erro de acesso à memória)
// T1: deveria verificar se a memória pertence ao processo
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender)
{
  for (int indice_str = 0; indice_str < tam; indice_str++)
  {
    int caractere;
    if (mem_le(mem, ender + indice_str, &caractere) != ERR_OK)
    {
      return false;
    }
    if (caractere < 0 || caractere > 255)
    {
      return false;
    }
    str[indice_str] = caractere;
    if (caractere == 0)
    {
      return true;
    }
  }
  // estourou o tamanho de str
  return false;
}

// vim: foldmethod=marker