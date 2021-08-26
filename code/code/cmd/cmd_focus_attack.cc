#include "handler.h"
#include "being.h"
#include "combat.h"
#include "obj_base_weapon.h"

int TBeing::doFocusAttack(const char *argument, TBeing *vict)
{
  const int FOCUS_ATTACK_MOVE = 10;
  TBeing *caster = this, *victim;
  
  if (checkBusy()) {
    return FALSE;
  }
  if (!doesKnowSkill(SKILL_FOCUS_ATTACK)) {
    sendTo("You know nothing about focused attacks.\n\r");
    return FALSE;
  }

  if (caster->checkPeaceful("You feel too peaceful to contemplate violence.\n\r"))
    return FALSE;

  // Adding a lockout 
  if (caster->affectedBySpell(SKILL_FOCUS_ATTACK)) {
    caster->sendTo("You are still recovering from your last focused attack and cannot use this ability again at this time.\n\r");
    return FALSE;
  }

  if (caster->getMove() < FOCUS_ATTACK_MOVE) {
    caster->sendTo("You don't have the vitality to make the move!\n\r");
    return FALSE;
  }

  if (!(caster->isImmortal() || IS_SET(caster->specials.act, ACT_IMMORTAL)))
    caster->addToMove(-FOCUS_ATTACK_MOVE);
  
  if (!(victim = vict) && 
     (!(victim = get_char_room_vis(this, argument))) && 
     (!(victim = fight()))) {
    sendTo("Who do you want to attack?\n\r");
    return FALSE;
  }

  if (!sameRoom(*victim)) { 
    sendTo("That person isn't around.\n\r");
    return FALSE;
  }
  
  if (victim->isImmortal() || IS_SET(victim->specials.act, ACT_IMMORTAL)) {
    sendTo("You cannot attack an immortal.\n\r");
    return FALSE;
  }
  
  affectedData aff1;
  aff1.type = SKILL_FOCUS_ATTACK;
  aff1.duration = Pulse::UPDATES_PER_MUDHOUR/2;
  aff1.bitvector = 0;
  caster->affectTo(&aff1, -1);

  int skillLevel = caster->getSkillValue(SKILL_FOCUS_ATTACK);
  int successfulSkill = caster->bSuccess(skillLevel, SKILL_FOCUS_ATTACK);

  if (!successfulSkill) {
    act("You attempt to perform a focused attack, but lose your concentration!", FALSE, caster, NULL, NULL, TO_CHAR);
    act("$n attempts to focus, but loses $s concentration.", FALSE, caster, NULL, NULL, TO_ROOM);

    return FALSE;
  }

  act("You focus on identifying a weakness in your opponent's defense.", FALSE, caster, NULL, NULL, TO_CHAR);
  act("$n concentrates on $s opponent, focusing intensely!", FALSE, caster, NULL, NULL, TO_ROOM);

  // Set the flag that we will later check for to trigger an attack
  SET_BIT(caster->specials.affectedBy, AFF_FOCUS_ATTACK);
  
  // If the characters aren't currently fighting, initiate combat
  if (!caster->fight()) {
    setCharFighting(victim);
    setVictFighting(victim);
    caster->reconcileHurt(victim, 0.01);
  }

  return TRUE;
}
