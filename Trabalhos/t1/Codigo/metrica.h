#ifndef METRICA_H
#define METRICA_H

#include "so.h"     
#include "processo.h"

// Funções de métricas
void inicializa_metricas(so_t *self);
void so_salva_metricas(so_t *self, const char *filename);

#endif // METRICA_H
