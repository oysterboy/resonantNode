#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ChirpOutput.h"
#include "OutputRequest.h"

/*
SoundOutput (OUT-002/003, step 10f; myspec 7.2)

Executes one OutputRequest at a time on the ChirpOutput and keeps an
OutputStatus. It owns the sequencing of a request: a hop train, a chirp
pattern, or the idle call (announcement tone, gap, then the node's call). It
checks a request against the output's physical limits before playing it.

Does NOT decide what to play or when: Behavior decides (shouldStartChirp(),
chirpPattern(), signalShape()) and the Node composes the concrete request
from that decision and the output.* params (the hop grid comes from the
detection band, which Behavior does not know). Moved here from the Node glue
(startEmission / updateEmissions) unchanged in timing.
*/

class SoundOutput {
public:
    // update() events, as bits.
    static constexpr uint8_t kEventCallStarted = 1;  // an IdleCall's call started (hop or plain chirp)
    static constexpr uint8_t kEventFinished = 2;     // the request ended (finishedAtMs())

    explicit SoundOutput(ChirpOutput& chirp, const OutputLimits& limits = OutputLimits());

    static OutputReject validate(const OutputRequest& request, const OutputLimits& limits);
    // The request's planned span: announcement + gap + call for an IdleCall
    // (plainCallMs when its call is the plain chirp), the train for a
    // HopTrain, 0 for a Pattern (ChirpOutput times it).
    static unsigned long plannedSpanMs(const OutputRequest& request, unsigned long plainCallMs);

    // Starts the request. False when rejected (status().lastReject says why);
    // nothing sounds then.
    bool start(const OutputRequest& request);
    // Advances the request once per loop; returns kEvent* bits.
    uint8_t update();
    // Ends whatever plays, also an IdleCall waiting in its gap. No event.
    void stop();

    bool busy() const { return _status.busy(); }
    unsigned long finishedAtMs() const { return _status.lastDoneMs; }
    const OutputStatus& status() const { return _status; }
    const OutputRequest& current() const { return _request; }
    // The hop train the call played or will play (for the log line).
    bool callIsHop() const { return _request.hopSteps > 0; }

private:
    bool startHop(const OutputRequest& request);
    bool startCall();
    void reject(OutputReject reason);
    void finish();

    ChirpOutput& _chirp;
    OutputLimits _limits;
    OutputRequest _request;
    OutputStatus _status;
    unsigned long _callAtMs = 0;
};
