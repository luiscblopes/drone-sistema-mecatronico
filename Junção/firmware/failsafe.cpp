/**
 * @file failsafe.cpp
 * @brief Implementação da máquina de failsafe com latch. NÃO-NEGOCIÁVEL.
 */
#include "failsafe.h"
#include "config.h"

#include <math.h>

/** Motivo corrente. */
static failsafe_reason_t s_reason = FS_NONE;
/** Latch: enquanto true, o rearme esta bloqueado. */
static bool s_latched = false;

/** @brief Indica se um motivo e perigoso (trava o rearme). */
static bool reason_is_dangerous(failsafe_reason_t r)
{
    /* Override manual e acao deliberada do operador: desabilita, mas NAO trava. */
    return (r != FS_NONE) && (r != FS_MANUAL_OVERRIDE);
}

void failsafe_init(void)
{
    s_reason = FS_NONE;
    s_latched = false;
}

bool failsafe_evaluate(const failsafe_input_t *in)
{
    if (in == NULL) { return false; }

    /* Enquanto travado, mantem o motivo perigoso e nega o acionamento. So
     * /stopAll (failsafe_clear_latch) destrava. */
    if (s_latched)
    {
        return false;
    }

    failsafe_reason_t r = FS_NONE;

    /* Ordem de prioridade: IMU > tilt > timeout (a IMU invalida torna tilt nao
     * confiavel; por isso e avaliada primeiro). */
    if ((!in->imu_valid) || (!in->imu_calibrated))
    {
        r = FS_IMU_INVALID;
    }
    else if ((fabsf(in->roll_deg) > MAX_SAFE_TILT_DEG) ||
             (fabsf(in->pitch_deg) > MAX_SAFE_TILT_DEG))
    {
        r = FS_EXCESSIVE_TILT;
    }
    else if (in->command_age_ms > FLIGHT_COMMAND_TIMEOUT_MS)
    {
        r = FS_COMMAND_TIMEOUT;
    }

    if (r != FS_NONE)
    {
        s_reason = r;
        s_latched = true; /* todas as condicoes acima sao perigosas: travam. */
        return false;
    }

    s_reason = FS_NONE;
    return true;
}

void failsafe_trip(failsafe_reason_t reason)
{
    if (reason_is_dangerous(reason))
    {
        s_reason = reason;
        s_latched = true;
    }
    else if (!s_latched)
    {
        /* Motivo nao-perigoso (ex.: override manual) so registra se NAO ha um
         * latch perigoso ativo — caso contrario mascararia a causa real na
         * telemetria (o operador precisa ver o perigo que travou o rearme). */
        s_reason = reason;
    }
}

void failsafe_clear_latch(void)
{
    s_reason = FS_NONE;
    s_latched = false;
}

failsafe_reason_t failsafe_reason(void)
{
    return s_reason;
}

bool failsafe_is_latched(void)
{
    return s_latched;
}

const char *failsafe_text(failsafe_reason_t reason)
{
    switch (reason)
    {
        case FS_NONE:            return "NONE";
        case FS_COMMAND_TIMEOUT: return "COMMAND_TIMEOUT";
        case FS_IMU_INVALID:     return "IMU_INVALID";
        case FS_EXCESSIVE_TILT:  return "EXCESSIVE_TILT";
        case FS_EMERGENCY_STOP:  return "EMERGENCY_STOP";
        case FS_MANUAL_OVERRIDE: return "MANUAL_OVERRIDE";
        default:                 return "UNKNOWN";
    }
}
