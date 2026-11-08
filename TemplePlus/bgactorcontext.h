#pragma once
#include <algorithm>
#include <vector>
// Query context only: the world's combat flag and initiative index never change.
// Explicit actor scopes nest, so a combat callback inside a realtime callback
// observes its own rules rather than inheriting the outer actor's mode.
template<class Actor> class BgActorContext {
public:
    class Scope {
    public:
        Scope(BgActorContext &context,Actor actor):context(context),previous(context.actor) {context.actor=actor;}
        ~Scope() {context.actor=previous;}
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
    private:
        BgActorContext &context;
        Actor previous;
    };
    template<class IsFree> bool Combat(bool globalCombat,IsFree isFree) const {
        return globalCombat && (actor==Actor{} || !isFree(actor));
    }
private:
    Actor actor{};
};

// Protect borrowed native slots while another actor allocates an action sequence.
template<class Sequence> class BgSequenceReservations {
public:
    class Scope {
    public:
        Scope(BgSequenceReservations &owner,Sequence *sequence):owner(owner),sequence(sequence) {
            if (sequence) owner.saved.push_back(sequence);
        }
        ~Scope() {if (sequence) owner.saved.pop_back();}
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
    private:
        BgSequenceReservations &owner;
        Sequence *sequence;
    };
    bool Contains(const Sequence *sequence) const {
        return std::find(saved.begin(),saved.end(),sequence)!=saved.end();
    }
private:
    std::vector<Sequence*> saved;
};
