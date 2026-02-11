/**
 * éterOS - Definiciones de Tipos Básicos
 * 
 * Tipos de datos fundamentales para un entorno freestanding
 * donde no están disponibles las cabeceras estándar de C.
 */

#ifndef ETEROS_TYPES_H
#define ETEROS_TYPES_H

/* ========================================================================= */
/* Tipos enteros con tamaño fijo                                             */
/* ========================================================================= */
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;

/* ========================================================================= */
/* Tipos de tamaño de plataforma                                             */
/* ========================================================================= */
typedef uint64_t            size_t;
typedef int64_t             ssize_t;
typedef uint64_t            uintptr_t;
typedef int64_t             intptr_t;

/* ========================================================================= */
/* Tipo booleano                                                             */
/* ========================================================================= */
typedef enum {
    false = 0,
    true  = 1
} bool;

/* ========================================================================= */
/* Constante NULL                                                            */
/* ========================================================================= */
#define NULL ((void*)0)

#endif /* ETEROS_TYPES_H */
