/* vim:set cin ft=c sw=4 sts=4 ts=8 et ai cino=Ls\:0t0(0 : -*- mode:c;fill-column:80;tab-width:8;c-basic-offset:4;indent-tabs-mode:nil;c-file-style:"k&r" -*-*/
/* Last modified by Fredrik Ljungdahl, 2015-11-03 */
/* Copyright (c) 1989 Mike Threepoint                             */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* Copyright (c) 2014 Alex Smith                                  */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "mfndpos.h"

/* This file is responsible for determining whether the character has intrinsics
   and extrinsics, because it was previously done with a bunch of macros, which
   are rather hard to iterate over, and make it even harder to work with the
   game's logic. */

static int race_from_pm(const struct permonst *);
static void init_permonsts(const struct monst *, const struct permonst **,
                           const struct permonst **, const struct permonst **);
static boolean is_green(struct monst *);
static boolean slip_or_trip(struct monst *);

/* Messages on intrinsic gain/loss */
/* Order: prop, outside, role, lost outside, lost role */

struct propmsg {
    unsigned int prop;
    const char *gainoutside, *gainpm, *loseoutside, *losepm;
};

static const struct propmsg prop_msg[] = {
    {FIRE_RES, "You feel a momentary chill.", "cool",
     "You feel warmer.", "warmer"},
    {COLD_RES, "You feel full of hot air.", "warm",
     "You feel cooler.", "cooler"},
    {SLEEP_RES, "You feel wide awake.", "awake",
     "You feel tired!", "tired"},
    {DISINT_RES, "You feel very firm.", "firm",
     "You feel less firm.", "less firm"},
    {SHOCK_RES, "Your health currently feels amplified!", "insulated",
     "You feel conductive.", "conductive"},
    {POISON_RES, "You feel healthy.", "hardy",
     "You feel a little sick!", "sickly"},
    {ACID_RES, "Your skin feels leathery.", "thick-skinned",
     "Your skin feels less leathery.", "soft-skinned"},
    {STONE_RES, "You feel extraordinarily limber.", "limber",
     "You feel stiff.", "stiff"},
    {SEARCHING, "", "perceptive", "", "unfocused"},
    {SEE_INVIS, "", "aware",
     "You thought you saw something!", "unaware"},
    {INVIS, "You feel hidden!", "hidden",
     "You feel paranoid.", "paranoid"},
    {TELEPORT, "You feel very jumpy.", "jumpy",
     "You feel less jumpy.", "less jumpy"},
    {TELEPORT_CONTROL, "You feel in control of yourself.", "controlled",
     "You feel less in control.", "uncontrolled"},
    {POLYMORPH, "Your body begins to shapeshift.", "shapeshifting",
     "You are no longer shapeshifting.", "less shapeshifting"},
    {POLYMORPH_CONTROL, "You feel in control of your shapeshifting", "shapeshift-controlled",
     "You feel no lnger in control of your shapeshifting.", "less shapeshift-controlled"},
    {STEALTH, "", "stealthy", "", "noisy"},
    {AGGRAVATE_MONSTER, "You feel attractive!", "attractive",
     "You feel less attractive.", "less attractive"},
    {WARNING, "", "sensitive", "", "insensitive"},
    {TELEPAT, "You feel a strange mental acuity.", "telepathic",
     "Your senses fail!", "untelepathic"},
    {FAST, "", "quick", "", "slow"},
    {SLEEPING, "You feel drowsy.", "drowsy", "You feel awake.", "awake"},
    {WWALKING, "You feel light on your feet.", "light",
     "You feel heavier.", "heavy"},
    {HUNGER, "You feel your metabolism speed up.", "hungry",
     "Your metabolism slows down.", "full."},
    {REFLECTING, "Your body feels repulsive.", "repulsive",
     "You feel less repulsive.", "absorptive"},
    {LIFESAVED, "You feel a strange sense of immortality.", "immortal",
     "You lose your sense of immortality!", "mortal"},
    {ANTIMAGIC, "You feel resistant to magic.", "skeptical",
     "Your magic resistance fails!", "credulous"},
    {DISPLACED, "Your outline shimmers and shifts.", "elusive",
     "You stop shimmering.", "exposed"},
    {SICK_RES, "You feel your immunity strengthen.", "immunized",
     "Your immunity system fails!", "immunocompromised"},
    {DRAIN_RES, "You feel especially energetic.", "energic",
     "You feel less energic.", "less energic"},
    {CANCELLED, "You feel devoid of magic!", "magic-devoid",
     "Your magic returns.", "magical"},
    {FREE_ACTION, "You feel especially agile.", "agile",
     "You feel less agile.", "less agile"},
    {SWIMMING, "You feel more attuned to water.", "water-attuned",
     "You forget your swimming skills.", "less water-attuned"},
    {FIXED_ABIL, "You feel resistant to exercise", "ability-fixed",
     "You feel less resistant to exercise", "ability-fixed"},
    {FLYING, "You feel more buoyant.", "buoyant",
     "You feel less buoyant.", "less buoyant"},
    {UNCHANGING, "You feel resistant to change.", "unchanged",
     "You feel less resistant to change.", "changed"},
    {PASSES_WALLS, "Your body unsolidifies", "unsolid",
     "Your body solidifies.", "solid"},
    {INFRAVISION, "", "vision-enhanced", "", "half blind"},
    {NO_PROP, "", "", "", ""}
};

static const struct propmsg prop_msg_hallu[] = {
    {FIRE_RES, "You be chillin'.", "cool",
     "You feel warmer.", "warmer"},
    {DISINT_RES, "You feel totally together, man.", "firm",
     "You feel split up.", "less firm"},
    {SHOCK_RES, "You feel grounded in reality.", "insulated",
     "You feel less grounded.", "conductive"},
    {SEE_INVIS, "", "attentive",
     "You tawt you taw a puttie tat!", "unattentive"},
    {TELEPORT, "You feel diffuse.", "jumpy",
     "You feel less jumpy.", "less jumpy"},
    {TELEPORT_CONTROL, "You feel centered in your personal space.", "controlled",
     "You feel less in control.", "uncontrolled"},
    {POLYMORPH, "You feel like a chameleon.", "shapeshifting",
     "You no longer feel like a chameleon.", "less shapeshifting"},
    {TELEPAT, "You feel in touch with the cosmos.", "telepathic",
     "Your cosmic connection is no more!", "untelepathic"},
    {WWALKING, "You feel like Jesus himself.", "light",
     "You realize that you aren't Jesus after all.", "heavy"},
    {DRAIN_RES, "You are bouncing off the walls!", "energetic",
     "You feel less bouncy.", "less energetic"},
    {FLYING, "You feel like a super hero!", "buoyant",
     "You sadly lose your heroic abilities.", "less buoyant"},
    {NO_PROP, "", "", "", ""}
};


/* Intrinsics roles gain by level up
   Yes, this makes it theoretically possible to give level-based
   intrinsics for monsters. This technique is not currently used
   besides the fact that player monsters are granted relevant
   intrinsics.
   XL1 intrinsics are stored in permonst (Adding XL1 properties
   here *works*, but is not the right way to do it IMO -FIQ).
   TODO: make this part of permonst... somehow
   TODO: if this thing ever grants fly/lev/similar, monster XL
   change needs to call update_property properly. */
struct propxl {
    unsigned int mnum;
    unsigned int xl;
    unsigned int prop;
};

static const struct propxl prop_from_experience[] = {
    {PM_ARCHEOLOGIST, 10, SEARCHING},
    {PM_BARBARIAN, 7, FAST},
    {PM_BARBARIAN, 15, STEALTH},
    {PM_CAVEMAN, 15, WARNING},
    {PM_HEALER, 15, WARNING},
    {PM_KNIGHT, 7, FAST},
    {PM_MONK, 3, POISON_RES},
    {PM_MONK, 5, STEALTH},
    {PM_MONK, 7, WARNING},
    {PM_MONK, 9, SEARCHING},
    {PM_MONK, 11, FIRE_RES},
    {PM_MONK, 13, COLD_RES},
    {PM_MONK, 15, SHOCK_RES},
    {PM_MONK, 17, TELEPORT_CONTROL},
    {PM_PRIEST, 15, WARNING},
    {PM_PRIEST, 20, FIRE_RES},
    {PM_RANGER, 7, STEALTH},
    {PM_RANGER, 15, SEE_INVIS},
    {PM_ROGUE, 10, SEARCHING},
    {PM_SAMURAI, 15, STEALTH},
    {PM_TOURIST, 10, SEARCHING},
    {PM_TOURIST, 20, POISON_RES},
    {PM_VALKYRIE, 7, FAST},
    {PM_WIZARD, 15, WARNING},
    {PM_WIZARD, 17, TELEPORT_CONTROL},
    {PM_ELF, 4, SLEEP_RES},
    {NON_PM, 0, 0}
};


/* Intrinsics for a certain monster form.
   Returns 1: yes, 0: no, -1: blocking. */
int
pm_has_property(const struct permonst *mdat, enum youprop property)
{
    uchar mfromrace = mdat->mresists;

    /* Blockers. Sickness resistance blocking sickness, etc, is handled
       elsewhere... */
    if (property == SLIMED            ? flaming(mdat) || unsolid(mdat) ||
                                        mdat == &mons[PM_GREEN_SLIME]        :
        property == STONED            ? poly_when_stoned(mdat)               :
        property == GLIB              ? nohands(mdat)                        :
        0)
        return -1;

    /* TODO: Change the monster data code into something that doesn't require a
       giant switch statement or ternary chain to get useful information from
       it. We use a ternary chain here because it cuts down on repetitive code
       and so is easier to read. */
    if (property == FIRE_RES          ? mfromrace & MR_FIRE                  :
        property == COLD_RES          ? mfromrace & MR_COLD                  :
        property == SLEEP_RES         ? mfromrace & MR_SLEEP                 :
        property == DISINT_RES        ? mfromrace & MR_DISINT                :
        property == SHOCK_RES         ? mfromrace & MR_ELEC                  :
        property == POISON_RES        ? mfromrace & MR_POISON                :
        property == ACID_RES          ? mfromrace & MR_ACID                  :
        property == STONE_RES         ? mfromrace & MR_STONE                 :
        property == DRAIN_RES         ? is_undead(mdat) || is_demon(mdat) ||
                                        is_were(mdat) ||
                                        mdat == &mons[PM_DEATH]              :
        property == SICK_RES          ? mdat->mlet == S_FUNGUS ||
                                        mdat == &mons[PM_GHOUL]              :
        property == ANTIMAGIC         ? dmgtype(mdat, AD_MAGM) ||
                                        dmgtype(mdat, AD_RBRE) ||
                                        mdat == &mons[PM_BABY_GRAY_DRAGON]   :
        property == STUNNED           ? mdat->mflags2 & M2_STUNNED           :
        property == BLINDED           ? !haseyes(mdat)                       :
        property == HALLUC            ? dmgtype(mdat, AD_HALU)               :
        property == SEE_INVIS         ? mdat->mflags1 & M1_SEE_INVIS         :
        property == TELEPAT           ? mdat->mflags2 & M2_TELEPATHIC        :
        property == INFRAVISION       ? pm_infravision(mdat)                 :
        property == INVIS             ? pm_invisible(mdat)                   :
        property == TELEPORT          ? mdat->mflags1 & M1_TPORT             :
        property == LEVITATION        ? is_floater(mdat)                     :
        property == FLYING            ? mdat->mflags1 & M1_FLY               :
        property == SWIMMING          ? pm_swims(mdat)                       :
        property == PASSES_WALLS      ? mdat->mflags1 & M1_WALLWALK          :
        property == REGENERATION      ? mdat->mflags1 & M1_REGEN             :
        property == REFLECTING        ? mdat == &mons[PM_SILVER_DRAGON]      :
        property == DISPLACED         ? mdat->mflags3 & M3_DISPLACED         :
        property == TELEPORT_CONTROL  ? mdat->mflags1 & M1_TPORT_CNTRL       :
        property == MAGICAL_BREATHING ? amphibious(mdat)                     :
        property == STEALTH           ? mdat->mflags3 & M3_STEALTHY          :
        property == FAST              ? mdat->mflags3 & M3_FAST              :
        property == SEARCHING         ? mdat->mflags3 & M3_SEARCH            :
        0)
        return 1;
    return 0;
}

/* Convert a permonst to a race where applicable. Used to determine
   source of property */
/* TODO: Real racial monsters */
static int
race_from_pm(const struct permonst *pm)
{
    return (pm->mflags2 & M2_HUMAN ? PM_HUMAN :
            pm->mflags2 & M2_ELF   ? PM_ELF   :
            pm->mflags2 & M2_DWARF ? PM_DWARF :
            pm->mflags2 & M2_GNOME ? PM_GNOME :
            pm->mflags2 & M2_ORC   ? PM_ORC   :
            0);
}

/* Initialize 3 permonsts set to role, race and poly. Used to determine
   source of properties (os_role, os_race, os_polyform) */
static void
init_permonsts(const struct monst *mon, const struct permonst **role,
               const struct permonst **race, const struct permonst **poly)
{
    int racenum;
    if (mon == &youmonst) {
        *role = &mons[urole.malenum];
        *race = &mons[urace.malenum];
    } else {
        if (mon->orig_mnum)
            *role = &mons[mon->orig_mnum];
        else
            *role = mon->data;
        racenum = race_from_pm(*role);
        if (racenum)
            *race = &mons[racenum];
    }
    if (*role != mon->data &&
        (mon != &youmonst || Upolyd)) { /* polymorphed */
        *poly = mon->data;
        *race = NULL; /* polymorph grants the polyform's race */
        racenum = race_from_pm(*poly);
        if (racenum)
            *race = &mons[racenum];
    }
}


/* Returns an object slot mask giving all the reasons why the given
   player/monster might have the given property, limited by "reasons", an object
   slot mask (W_EQUIP, INTRINSIC, and ANY_PROPERTY are the most likely values
   here, but you can specify slots individually if you like).
   The "os_polyform" checks used to not be accurate at all. They are now,
   hopefully */
unsigned
m_has_property(const struct monst *mon, enum youprop property,
               unsigned reasons, boolean even_if_blocked)
{
    /* Monsters can't hallucinate at present */
    if (property == HALLUC && mon != &youmonst)
        return 0;

    unsigned rv = 0;
    const struct permonst *mdat_role = NULL;
    const struct permonst *mdat_race = NULL;
    const struct permonst *mdat_poly = NULL;
    init_permonsts(mon, &mdat_role, &mdat_race, &mdat_poly);

    /* The general case for equipment */
    rv |= mworn_extrinsic(mon, property);

    /* Timed and corpse/etc-granted */
    if (mon->mintrinsic[property] & TIMEOUT)
        rv |= W_MASK(os_timeout);
    if (mon->mintrinsic[property] & FROMOUTSIDE_RAW)
        rv |= W_MASK(os_outside);

    /* Polyform / role / race properties */
    const struct propxl *pmprop;
    for (pmprop = prop_from_experience; pmprop->mnum != NON_PM;
         pmprop++) {
        if (pmprop->prop == property &&
            pmprop->xl <= (mon == &youmonst ? u.ulevel : mon->m_lev)) {
            if (pmprop->mnum == monsndx(mdat_role))
                rv |= W_MASK(os_role);
            if (mdat_race && pmprop->mnum == monsndx(mdat_race))
                rv |= W_MASK(os_race);
            if (mdat_poly && pmprop->mnum == monsndx(mdat_poly))
                rv |= W_MASK(os_polyform);
        }
    }
    if (pm_has_property(mdat_role, property) > 0)
        rv |= W_MASK(os_role);
    if (mdat_race && pm_has_property(mdat_race, property) > 0)
        rv |= W_MASK(os_race);
    if (mdat_poly && pm_has_property(mdat_poly, property) > 0)
        rv |= W_MASK(os_polyform);

    /* External circumstances */
    /* Fumbling on ice */
    if (property == FUMBLING &&
        is_ice(mon == &youmonst ? level : mon->dlevel,
               m_mx(mon), m_my(mon)) &&
        !levitates(mon) && !flying(mon) &&
        !is_whirly(mon->data)) {
        struct obj *armf = which_armor(mon, os_armf);
        if (armf && armf->otyp == find_skates())
            rv |= W_MASK(os_circumstance);
    }

    /* Cases specific to the player */
    if (mon == &youmonst) {
        /* Birth options */
        if (property == BLINDED && flags.permablind)
            rv |= W_MASK(os_birthopt);
        if (property == HALLUC && flags.permahallu)
            rv |= W_MASK(os_birthopt);
        if (property == UNCHANGING && flags.polyinit_mnum != -1)
            rv |= W_MASK(os_birthopt);

        /* External circumstances */
        if (property == BLINDED && u_helpless(hm_unconscious))
            rv |= W_MASK(os_circumstance);

        /* Riding */
        if (property == FLYING && u.usteed && is_flyer(u.usteed->data))
            rv |= W_MASK(os_saddle);
        if (property == SWIMMING && u.usteed && pm_swims(u.usteed->data))
            rv |= W_MASK(os_saddle);
    }

    /* Overrides

       TODO: Monsters with no eyes are not considered blind. This doesn't
       make much sense. However, changing it would be a major balance
       change (due to Elbereth), and so it has been left alone for now. */
    if (property == BLINDED && !haseyes(mon->data))
        rv |= (unsigned)(W_MASK(os_blocked));
    if (property == HALLUC && resists_hallu(mon))
        rv |= (unsigned)(W_MASK(os_blocked));
    if (property == WWALKING && Is_waterlevel(m_mz(mon)))
        rv |= (unsigned)(W_MASK(os_blocked));
    if (mworn_blocked(mon, property))
        rv |= (unsigned)(W_MASK(os_blocked));

    /* If a property is blocked, turn off all flags except circumstance/blocked,
       unless even_if_blocked is TRUE */
    if ((rv & W_MASK(os_blocked)) && !even_if_blocked)
        rv &= (unsigned)(W_MASK(os_circumstance) |
                         W_MASK(os_birthopt));
    return rv & reasons;
}


/* Check if an object/spell/whatever would have any effect on a target */
boolean
obj_affects(const struct monst *user, struct monst *target, struct obj *obj)
{
    int wandlevel;
    switch (obj->otyp) {
    case WAN_FIRE:
    case SPE_FIREBALL:
    case SCR_FIRE:
        return (!prop_wary(user, target, FIRE_RES) ||
                prop_wary(user, target, SLIMED));
    case WAN_COLD:
    case SPE_CONE_OF_COLD:
        return !prop_wary(user, target, COLD_RES);
    case POT_SLEEPING:
        if (!prop_wary(user, target, FREE_ACTION))
            return TRUE;
        /* fallthrough */
    case WAN_SLEEP:
    case SPE_SLEEP:
        return !prop_wary(user, target, SLEEP_RES);
    case WAN_LIGHTNING:
        return !prop_wary(user, target, SHOCK_RES);
    case SCR_STINKING_CLOUD:
        /* technically also cause blindness, but for like 3 turns... */
    case POT_SICKNESS:
        return !prop_wary(user, target, POISON_RES);
    case POT_ACID:
        return !prop_wary(user, target, ACID_RES);
    case SPE_STONE_TO_FLESH:
        return (prop_wary(user, target, STONED) ||
                target->data == &mons[PM_STONE_GOLEM]);
    case EGG:
        /* trice eggs only */
        if (!touch_petrifies(&mons[obj->corpsenm]))
            return FALSE;
        return (!prop_wary(user, target, STONE_RES) ||
                target->data == &mons[PM_FLESH_GOLEM]);
    case WAN_MAKE_INVISIBLE:
        /* skilled users of /oInvis can uninvis */
        wandlevel = 0;
        if (obj->oclass == WAND_CLASS) {
            wandlevel = mprof(user, MP_WANDS);
            if (obj->mbknown)
                wandlevel = getwandlevel(user, obj);
            if (wandlevel >= P_SKILLED)
                return TRUE;
        }
        return !prop_wary(user, target, INVIS);
    case WAN_POLYMORPH:
    case SPE_POLYMORPH:
    case POT_POLYMORPH:
        return !(prop_wary(user, target, UNCHANGING) ||
                 prop_wary(user, target, ANTIMAGIC));
    case WAN_STRIKING:
    case SPE_FORCE_BOLT:
        return !prop_wary(user, target, ANTIMAGIC);
    case WAN_MAGIC_MISSILE:
    case SPE_MAGIC_MISSILE:
        if (!prop_wary(user, target, ANTIMAGIC))
            return TRUE;
        if (!user)
            return FALSE;
        wandlevel = 0;
        if (obj->oclass == WAND_CLASS) {
            wandlevel = mprof(user, MP_WANDS);
            if (obj->mbknown)
                wandlevel = getwandlevel(user, obj);
            if (wandlevel >= P_SKILLED)
                return TRUE;
        }
        return FALSE;
    case WAN_SLOW_MONSTER:
    case SPE_SLOW_MONSTER:
        return !prop_wary(user, target, SLOW);
    case WAN_SPEED_MONSTER:
        /* a monster might not know if a target is fast, but
           if not, he'd find that out rather fast */
        return !very_fast(target);
    case WAN_UNDEAD_TURNING:
    case SPE_TURN_UNDEAD:
        return is_undead(target->data);
    case WAN_CANCELLATION:
    case SPE_CANCELLATION:
        return !prop_wary(user, target, CANCELLED);
    case WAN_DEATH:
    case SPE_FINGER_OF_DEATH:
        if (!(prop_wary(user, target, ANTIMAGIC) ||
              is_undead(target->data) ||
              is_demon(target->data)))
            return TRUE;
        if (!user)
            return FALSE;
        wandlevel = 0;
        if (obj->oclass == WAND_CLASS) {
            wandlevel = mprof(user, MP_WANDS);
            if (obj->mbknown)
                wandlevel = getwandlevel(user, obj);
            if (wandlevel >= P_EXPERT)
                return !prop_wary(user, target, DRAIN_RES);
        }
        return FALSE;
    case POT_PARALYSIS:
        return !prop_wary(user, target, FREE_ACTION);
    case POT_CONFUSION:
        return !prop_wary(user, target, CONFUSION);
    case POT_BLINDNESS:
        return !prop_wary(user, target, BLINDED);
    case SPE_HEALING:
    case SPE_EXTRA_HEALING:
        /* healing/extra healing cures blindness unless selfzapped */
        if ((!user || user != target) &&
            prop_wary(user, target, BLINDED))
            return TRUE;
        if (target == &youmonst && Upolyd)
            return (u.mh < u.mhmax);
        return (m_mhp(target) < m_mhpmax(target));
    case SPE_DRAIN_LIFE:
        return !prop_wary(user, target, DRAIN_RES);
    }
    return TRUE;
}

boolean
prop_wary(const struct monst *mon, struct monst *target, enum youprop prop)
{
    /* If !mon, or for some properties that is always announced,
       or for allies/peacefuls, or for WoY, always be accurate */
    if (!mon ||
        prop == INVIS || /* "the invisible X" */
        prop == AGGRAVATE_MONSTER || /* "seen: aggravate monster" */
        (target == &youmonst && mon->mpeaceful) ||
        (target != &youmonst && mon->mpeaceful == target->mpeaceful) ||
        mon->iswiz || mon == target || mon->mtame)
        return (m_has_property(target, prop, ANY_PROPERTY, TRUE));
    /* Monsters always know properties gained from those */
    if (m_has_property(target, prop,
                       (W_MASK(os_polyform) | W_MASK(os_birthopt) |
                        W_MASK(os_role) | W_MASK(os_race)), TRUE))
        return TRUE;

    /* avoid monsters trying something futile */
    if (mworn_blocked(target, prop))
        return TRUE;

    /* TODO: make monsters learn properties properly */
    if (rn2(4))
        return (has_property(target, prop));
    return FALSE;
}

int
property_timeout(struct monst *mon, enum youprop property)
{
    return mon->mintrinsic[property] & TIMEOUT;
}


void
decrease_property_timers(struct monst *mon)
{
    enum youprop prop;
    int skill = 0;
    skill = (mon == &youmonst ? P_SKILL(P_CLERIC_SPELL) :
             mprof(mon, MP_SCLRC));
    for (prop = 0; prop <= LAST_PROP; prop++) {
        if (mon->mintrinsic[prop] & TIMEOUT) {
            /* Decrease protection at half speed at Expert */
            if (prop == PROTECTION && skill == P_EXPERT &&
                (moves % 2))
                continue;
            mon->mintrinsic[prop]--;
            update_property(mon, prop, os_dectimeout);
        }
    }
}


/* Can this monster teleport at will?
   Any monster who has reached XL12 or more can teleport at will if they have teleportitis.
   If the monster has teleportitis in their natural form, they can always teleport at will.
   If the monster is a wizard, they can teleport at will from XL8 with teleportitis. */
boolean
teleport_at_will(const struct monst *mon)
{
    if (!teleportitis(mon))
        return FALSE;
    if (teleportitis(mon) & W_MASK(os_polyform))
        return TRUE;
    int level;
    if (mon == &youmonst)
        level = u.ulevel;
    else
        level = mon->m_lev;
    if (level >= 12)
        return TRUE;
    if (level >= 8) {
        if (mon == &youmonst && Race_if(PM_WIZARD))
            return TRUE;
        if (mon != &youmonst && spellcaster(mon->data))
            return TRUE;
        return FALSE;
    }
    return FALSE;
}
    
/* Checks whether or not a monster has controlled levitation.
   "Controlled" levitation here means that the monster can
   end it on its' own accord. include_extrinsic also includes
   extrinsics. "why" makes this function return the reason
   for the uncontrolled levitation or 0 if it is, in fact,
   controlled (or non-existent). */
unsigned
levitates_at_will(const struct monst *mon, boolean include_extrinsic,
    boolean why)
{
    unsigned lev = levitates(mon);
    unsigned lev_worn = mworn_extrinsic(mon, LEVITATION);

    /* polyform */
    if (is_floater(mon->data))
        return (why ? W_MASK(os_polyform) : 0);

    /* uncontrolled intrinsic levitation */
    if ((lev & lev_worn) && !(lev & W_MASK(os_outside)))
        return (why ? (lev & lev_worn) : 0);

    /* has extrinsic */
    if ((lev & lev_worn) && !include_extrinsic)
        return (why ? lev_worn : 0);

    if (lev_worn) { /* armor/ring/slotless levitation active */
        struct obj *chain = m_minvent(mon);
        int warntype;
        long itemtype;
        
        while (chain) {
            /* worn item or slotless unremoveable item */
            itemtype = item_provides_extrinsic(chain, LEVITATION, &warntype);
            if (itemtype && chain->cursed && (chain->owornmask ||
                (itemtype == W_MASK(os_carried) && chain->otyp == LOADSTONE)))
                return (why ? itemtype : 0);
            chain = chain->nobj;
        }
    }
    
    return lev;
}

/* Used when monsters need to abort levitation for some reason.
   (0=no turn spent, 1=turn spent, 2=died) */
unsigned
mon_remove_levitation(struct monst *mon, boolean forced)
{
    unsigned lev_source = levitates_at_will(mon, TRUE, FALSE);
    if (!lev_source) {
        lev_source = levitates(mon);
        if (!forced)
            return 0;
    }

    /* equavilent to cancelling levi with > as player */
    if (lev_source & FROMOUTSIDE) {
        set_property(mon, LEVITATION, -2, forced);
        lev_source = levitates(mon);
        if (!forced)
            return DEADMONSTER(mon) ? 2 : 1;
    }

    /* monster levitation comes from an extrinsic */
    struct obj *chain = m_minvent(mon);
    int warntype;
    long itemtype;
    int slot;
    boolean dropped; /* Monsters can drop several items in a single turn,
                        but if it drops any items, it can't do stuff
                        beyond that */
    while (chain) {
        itemtype = item_provides_extrinsic(chain, LEVITATION, &warntype);
        if (itemtype) {
            if (chain->owornmask && (!dropped || forced)) {
                slot = chain->owornmask;
                if (forced) {
                    chain->owornmask = 0;
                    mon->misc_worn_check &= ~W_MASK(slot);
                } else
                    return equip(mon, chain, FALSE, TRUE);
            } if (itemtype == W_MASK(os_carried)) {
                if (forced)
                    mdrop_obj(mon, chain, FALSE);
                else if (chain->otyp == LOADSTONE && chain->cursed)
                    return 0;
                else {
                    mdrop_obj(mon, chain, TRUE);
                    dropped = TRUE;
                }
            }
        }
        chain = chain->nobj;
    }
    
    if (!forced || levitates(mon)) {
        /* at this point, only polyform levitation is left */
        if (forced) {
            if (cansee(mon->mx, mon->my))
                pline("%s wobbles unsteadily for a moment.", Monnam(mon));
        }
        return dropped ? 1 : 0;
    }
    
    if (lev_source) {
        if (cansee(mon->mx, mon->my))
            pline("%s crashes to the floor!", Monnam(mon));

        mon->mhp -= rn1(8, 14); /* same as for player with 11 Con */
        if (mon->mhp <= 0) {
            if (cansee(mon->mx, mon->my))
                pline("%s dies!", Monnam(mon));
            else if (mon->mtame)
                pline("You have a sad feeling for a moment, then it passes.");
            mondied(mon);
        }
    }
    return 0;
}

/* Gremlin attack. Removes a random intrinsic. */
void
gremlin_curse(struct monst *mon)
{
    int i;
    enum youprop prop;
    for (i = 0; i < 200; i++) {
        prop = rnd(LAST_PROP);
        if (m_has_property(mon, prop, W_MASK(os_outside), TRUE)) {
            set_property(mon, prop, -1, FALSE);
            return;
        }
    }
    if (mon == &youmonst || canseemon(mon))
        pline("But nothing happens.");
    return;
}

/* Sets a property.
   type>0: Set a timeout
   type=0: Set os_outside
   type-1: Remove os_outside
   type-2: Remove os_outside and the timer
   forced will bypass update_property(). It is used
   when a special case is needed, and code will have
   to handle the work related to the property itself.
   Note that "set a timeout" literally sets whatever you specify.
   If you want to increase the timeout (potentially from 0),
   use inc_property(). */
boolean
set_property(struct monst *mon, enum youprop prop,
             int type, boolean forced)
{
    boolean increased = FALSE;
    if (mon->mintrinsic[prop] & TIMEOUT && type > 0)
        increased = TRUE;

    /* check for redundant usage */
    if (type == 0 && m_has_property(mon, prop, W_MASK(os_outside), TRUE))
        return FALSE;
    if (type == -1 && !m_has_property(mon, prop, W_MASK(os_outside), TRUE))
        return FALSE;
    if (type == -2 &&
        !m_has_property(mon, prop, (W_MASK(os_outside) | W_MASK(os_timeout)), TRUE))
        return FALSE;

    if (type > 0) { /* set timeout */
        mon->mintrinsic[prop] &= ~TIMEOUT;
        mon->mintrinsic[prop] |= min(type, TIMEOUT);
    } else if (type == 0) /* set outside */
        mon->mintrinsic[prop] |= FROMOUTSIDE_RAW;
    else { /* unset outside */
        mon->mintrinsic[prop] &= ~FROMOUTSIDE_RAW;
        if (type == -2) /* ...and timeout */
            mon->mintrinsic[prop] &= ~TIMEOUT;
    }

    if (forced)
        return FALSE;

    if (type > 0 || type == -2) {
        if (increased)
            return update_property(mon, prop, os_inctimeout);
        else
            return update_property(mon, prop, os_timeout);
    } else
        return update_property(mon, prop, os_outside);
}

boolean
inc_timeout(struct monst *mon, enum youprop prop,
            int time, boolean forced)
{
    return set_property(mon, prop,
                        min(time + property_timeout(mon, prop),
                            TIMEOUT), forced);
}

/* Called on experience level changes */
void
update_xl_properties(struct monst *mon, int oldlevel)
{
    enum objslot slot;
    int newlevel = u.ulevel;
    if (mon != &youmonst)
        newlevel = mon->m_lev;

    const struct permonst *mdat_role = NULL;
    const struct permonst *mdat_race = NULL;
    const struct permonst *mdat_poly = NULL;
    init_permonsts(mon, &mdat_role, &mdat_race, &mdat_poly);

    const struct propxl *pmprop;
    for (pmprop = prop_from_experience; pmprop->mnum != NON_PM;
         pmprop++) {
        slot = os_invalid;
        /* Run update_property() for ones acquired/lost between the
           levels gained/lost. Skip the min level since the hero is
           sure to have that regardless */
        if (pmprop->xl <= max(oldlevel, newlevel) &&
            pmprop->xl > min(oldlevel, newlevel)) {
            if (pmprop->mnum == monsndx(mon->data))
                slot = os_role;
            if (mdat_race && pmprop->mnum == monsndx(mdat_race))
                slot = os_race;
            if (mdat_poly && pmprop->mnum == monsndx(mdat_poly))
                slot = os_polyform;
            if (slot != os_invalid)
                update_property(mon, pmprop->prop, slot);
        }
    }
}

/* Called on polyself to possibly do some extra work for some properties.
   Returns a monster index if that should override the current polymorph
   (used if you polymorph into a golem while petrifying). */
int
update_property_polymorph(struct monst *mon, int pm)
{
    int pmcur = monsndx(mon->data);
    enum youprop prop;
    boolean hasprop, hasprop_poly, pm_hasprop, pm_blocks;
    for (prop = 0; prop <= LAST_PROP; prop++) {
        /* Permonst-specific blocks only happen for sliming/petrification, so
           bypassing update checks alltogether if a monster currently blocks is OK */
        if (m_has_property(mon, prop, ANY_PROPERTY, TRUE) & W_MASK(os_blocked))
            continue;

        hasprop = !!has_property(mon, prop);
        hasprop_poly = hasprop && !(has_property(mon, prop) & ~W_MASK(os_polyform));
        pm_hasprop = pm_has_property(&mons[pm], prop) > 0;
        pm_blocks = pm_has_property(&mons[pm], prop) < 0;
        if ((hasprop && pm_blocks) || /* has property, target blocks */
            (hasprop_poly && !pm_hasprop) || /* has property from polyself only, target lacks */
            (!hasprop && pm_hasprop)) /* lacks property, target has */
            update_property(mon, prop, os_newpolyform);

        /* polymorphed as a result, bail out since this might no longer be relevant
           (the polymorph, if any happened, will have run this again anyway) */
        if (pmcur != monsndx(mon->data))
            return monsndx(mon->data);
    }
    return 0;
}

/* Called to give any eventual messages and perform checks in case
   e.g. mon lost levitation (drowning), stone res (wielding trice).
   TODO: some of the status problem message logic is a mess, fix it */
boolean
update_property(struct monst *mon, enum youprop prop,
                enum objslot slot)
{
    /* Items call update_property() when lost, whether or not it had a property */
    if (prop == NO_PROP)
        return FALSE;

    /* update_property() can run for monsters wearing armor during level creation,
       or potentially off-level, so level can be non-existent or outright wrong,
       take this into account when messing with this function */
    boolean offlevel = (!level || level != mon->dlevel);
    boolean vis = !offlevel && canseemon(mon);
    /* Used when the updating is related to monster invisibility
       since canseemon() wont work if the monster just turned
       itself invisible */
    boolean vis_invis = !offlevel && cansee(mon->mx, mon->my);
    /* if slot is inctimeout or newpolyform, point real_slot to
       timeout or polyform respectively -- new* is to give proper messages */
    int real_slot = (slot == os_inctimeout  ? os_timeout  :
                     slot == os_dectimeout  ? os_timeout  :
                     slot == os_newpolyform ? os_polyform :
                     slot);
    boolean lost = !(has_property(mon, prop) & W_MASK(real_slot));
    boolean blocked;
    blocked = !!(m_has_property(mon, prop, ANY_PROPERTY, TRUE) & W_MASK(os_blocked));
    /* Whether or not a monster has it elsewhere */
    boolean redundant = !!(has_property(mon, prop) & ~W_MASK(real_slot));
    /* make a redundant flag accurate for speed changes... */
    boolean redundant_intrinsic = FALSE;
    if (((W_MASK(real_slot) & INTRINSIC) &&
         (has_property(mon, prop) & INTRINSIC & ~W_MASK(real_slot))) ||
        ((W_MASK(real_slot) & ~INTRINSIC) &&
         (has_property(mon, prop) & ~INTRINSIC & ~W_MASK(real_slot))))
        redundant_intrinsic = TRUE;
    /* Special case: set redundant to whether or not the monster has the property
       if we're dealing with (inc|dec)timeout */
    if (slot == os_inctimeout || slot == os_dectimeout) {
        redundant = !!has_property(mon, prop);
        redundant_intrinsic = !!(has_property(mon, prop) & ~INTRINSIC);
    }

    /* Hallu checks *your* hallucination since it's used for special
       messages */
    boolean hallu = hallucinating(&youmonst);
    boolean you = (mon == &youmonst);
    /* if something was said about the situation */
    boolean effect = FALSE;
    int timer = property_timeout(mon, prop);
    struct obj *weapon;

    /* Messages when properties are acquired/lost */
    if (mon == &youmonst &&
        (slot == os_role || slot == os_race ||
         slot == os_polyform || slot == os_outside)) {
        const struct propmsg *msg;
        if (hallu) {
            for (msg = prop_msg_hallu; msg->prop != NO_PROP; msg++) {
                if (msg->prop == prop) {
                    /* the XL-based properties always use "You feel ...!" */
                    if (slot != os_outside)
                        pline("You feel %s!",
                              lost ? msg->losepm : msg->gainpm);
                    else
                        pline("%s",
                              lost ? msg->loseoutside : msg->gainoutside);
                    effect = TRUE;
                    break;
                }
            }
        }
        if (!effect) { /* effect means a hallu msg was printed already */
            for (msg = prop_msg; msg->prop != NO_PROP; msg++) {
                if (msg->prop == prop) {
                    if (slot != os_outside)
                        pline("You feel %s!",
                              lost ? msg->losepm : msg->gainpm);
                    else
                        pline("%s",
                              lost ? msg->loseoutside : msg->gainoutside);
                    effect = TRUE;
                    break;
                }
            }
        }
    }

    /* Additional work for gained/lost properties. Properties are in order, hence
       some redundant breaks. */
    /* TODO: This logic, especially considering os_(inc|dec)timeout could use some
       prettifying */
    switch (prop) {
    case FIRE_RES:
        /* BUG: shouldn't there be a check for lava here?
        if (lost && !redundant) {
        } */
    case COLD_RES:
    case SLEEP_RES:
    case DISINT_RES:
    case SHOCK_RES:
    case POISON_RES:
    case ACID_RES:
        break;
    case STONE_RES:
        weapon = m_mwep(mon);
        if (lost && !redundant && weapon &&
            weapon->otyp == CORPSE &&
            touch_petrifies(&mons[weapon->corpsenm])) {
            if (!you)
                mselftouch(mon, "No longer petrify-resistant, ",
                           !flags.mon_moving);
            else {
                const char *kbuf;
                kbuf = msgprintf("losing stone resistance while wielding %s",
                                 urace.adj);
                selftouch("No longer petrify-resistant, you", kbuf);
            }
            if (!resists_ston(mon)) { /* lifesaved */
                if (!you) {
                    setmnotwielded(mon, mon->mw);
                    MON_NOWEP(mon);
                } else
                    uwepgone();
            }
            if (you || vis)
                effect = TRUE;
        } else if (!lost && !redundant)
            set_property(mon, STONED, -2, FALSE);
        break;
    case ADORNED:
    case REGENERATION:
    case SEARCHING:
        break;
    case SEE_INVIS:
        if (you) {
            set_mimic_blocking();       /* do special mimic handling */
            see_monsters(FALSE);        /* see invisible monsters */
            newsym(u.ux, u.uy);         /* see yourself! */
            if (!redundant && invisible(mon)) {
                pline(lost ? "Your body seems to fade out." :
                      "You can see yourself, but remain transparent.");
                effect = TRUE;
            }
        }
        break;
    case INVIS:
        if (you) {
            if (!redundant) {
                if (lost)
                    pline("Your body seems to unfade...");
                else
                    pline("%s %s.",
                          hallu ? "Far out, man!  You" :
                          "Gee!  All of a sudden, you",
                          see_invisible(&youmonst) ?
                          "can see right through yourself" :
                          "can't see yourself");
                effect = TRUE;
            }
            newsym(u.ux, u.uy);
        } else if (!redundant && vis_invis) {
            if (see_invisible(&youmonst)) {
                pline(lost ? "%s body seems to unfade..." :
                      "%s body turns transparent!",
                      s_suffix(Monnam(mon)));
            } else {
                /* call x_monnam directly to get rid of "The invisible ..." */
                pline(lost ? "%s appears!" :
                      (msensem(&youmonst, mon) & MSENSE_ANYDETECT) ?
                      "%s disappears, but you can still sense it." :
                      "%s suddenly disappears!",
                      msgupcasefirst(x_monnam(mon, ARTICLE_THE, NULL,
                                              (mon->mnamelth ? SUPPRESS_SADDLE : 0) |
                                              SUPPRESS_IT | SUPPRESS_INVISIBLE,
                                              FALSE)));
                set_mimic_blocking();       /* do special mimic handling */
                see_monsters(FALSE);        /* see invisible monsters */
            }
            effect = TRUE;
        }
        break;
    case TELEPORT:
        if (you)
            update_supernatural_abilities();
        break;
    case TELEPORT_CONTROL:
    case POLYMORPH:
    case POLYMORPH_CONTROL:
        break;
    case LEVITATION:
        if (lost && real_slot == os_timeout)
            set_property(mon, LEVITATION, -1, TRUE);

        /* this isn't really the right place, but there isn't any better place...
           reset levi_wary */
        if (slot != os_dectimeout && !you)
            mon->levi_wary = 0;

        if (!redundant) {
            if (!lost)
                float_up(mon);
            else
                float_down(mon);
            if (vis || you)
                effect = TRUE;
        }
        break;
    case STEALTH:
        if (slot == os_armf && !redundant &&
            !levitates(mon) && !flying(mon)) {
            if (you)
                pline(lost ? "You sure are noisy." :
                      "You walk very quietly");
            else if (vis)
                pline(lost ? "%s sure is noisy." :
                      "%s walks very quietly.",
                      Monnam(mon));
            effect = TRUE;
        }
        break;
    case AGGRAVATE_MONSTER:
        if (!you && !redundant) {
            you_aggravate(mon);
            see_monsters(FALSE);
        }
        break;
    case CONFLICT:
        /* Monsters should not be causing conflict. Just
           in case it happens anyway, alert the player. */
        if (!you) {
            pline(lost ? "You feel as if a conflict disappeared." :
                  "You feel as if someone is causing conflict.");
            effect = TRUE;
        }
        break;
    case PROTECTION:
        if (you && slot == os_armc && !lost) {
            pline("Your cloak feels unusually protective.");
            effect = TRUE;
        } else if (slot == os_dectimeout && !(timer % 10) &&
                   (you || vis)) {
            pline("The %s haze around %s %s.", hcolor("golden"),
                  you ? "you" : mon_nam(mon),
                  m_mspellprot(mon) ? "becomes less dense" : "disappears");
            effect = TRUE;
        }
        break;
    case PROT_FROM_SHAPE_CHANGERS:
        if (!redundant && lost)
            restartcham();
        else
            resistcham();
        break;
    case WARNING:
    case TELEPAT:
        if (you)
            see_monsters(FALSE);
        break;
    case FAST:
        /* only give the "new energy" message if the monster has redundant speed */
        if (redundant_intrinsic) {
            if (slot == os_inctimeout && you) {
                pline("Your %s get new energy.",
                      makeplural(body_part(LEG)));
                effect = TRUE;
            }
            break;
        }
        /* if "redundant" is set at this point, it is pointing
           at speed of the "other" kind (very fast if intrinsic, fast if extrinsic) */

        /* speed boots */
        if (slot == os_armf) {
            if (you || vis) {
                pline("%s %s %s%s.",
                      you ? "You" : Monnam(mon),
                      you ? "feel yourself" : "seems to",
                      lost ? "slow down" : "speed up",
                      redundant && lost ? " slightly" :
                      redundant ? " a bit more" : "");
                effect = TRUE;
            }
            break;
        }

        /* general (non-speed-boots) speed loss */
        if (lost) {
            if (slot == os_outside && redundant) {
                if (you) {
                    pline("Your quickness feels less natural.");
                    effect = TRUE;
                }
                break;
            }

            if (you || vis) {
                pline("%s slow%s down%s.",
                      you ? "You" : Monnam(mon),
                      you ? "" : "s",
                      redundant && (W_MASK(real_slot) & ~INTRINSIC) ? " slightly" : "");
                effect = TRUE;
            }
            break;
        }

        /* intrinsic acquirement */
        if (slot == os_outside) {
            if (!redundant && (you || vis)) {
                pline("%s speed%s up.",
                      you ? "You" : Monnam(mon),
                      you ? "" : "s");
                effect = TRUE;
            } else if (you) {
                pline("Your quickness feels more natural.");
                effect = TRUE;
            }
            break;
        }

        if (real_slot & ~INTRINSIC) {
            if (slot != os_inctimeout && !redundant_intrinsic) {
                if (you || vis) {
                    pline("%s %s moving %sfaster.",
                          you ? "You" : Monnam(mon),
                          you ? "are suddenly" : "seems to be",
                          redundant ? "" : "much ");
                    effect = TRUE;
                }
            }
        }
        break;
    case STUNNED:
        if (you || vis) {
            if (redundant) {
                if (slot == os_inctimeout) {
                    if (you)
                        pline(hallu ? "You feel like wobbling some more." :
                              "You struggle to keep your balance.");
                    else
                        pline("%s struggles to keep %s balance.",
                              Monnam(mon), mhis(mon));
                    effect = TRUE;
                }
            } else {
                if (lost)
                    pline("%s %s %s now.",
                          you ? "You" : Monnam(mon),
                          you ? "feel" : "looks",
                          you && hallu ? "less wobbly" : "a bit steadier");
                else
                    pline("%s %s%s...",
                          you ? "You" : Monnam(mon),
                          you && hallu ? "wobble" : stagger(mon->data, "stagger"),
                          you ? "" : "s");
                effect = TRUE;
            }
        }
        break;
    case CONFUSION:
        if (you) {
            if (lost && !redundant) {
                pline(hallu ? "You feel less trippy." :
                      "You are no longer confused.");
                effect = TRUE;
            } else if (redundant) {
                if (slot == os_inctimeout) {
                    pline("You are even more %s",
                          hallu ? "trippy!" : "confused...");
                    effect = TRUE;
                }
            } else {
                pline(hallu ? "What a trippy feeling!" :
                      "Huh, What?  Where am I?");
                effect = TRUE;
            }
        } else if (vis && (!redundant || slot == os_inctimeout)) {
            pline(redundant ? "%s looks even more confused..." :
                  lost ? "%s looks less confused now." :
                  "%s looks rather confused.", Monnam(mon));
            effect = TRUE;
        }
        break;
    case SICK:
        if (lost && slot == os_dectimeout) {
            if (you || vis) {
                pline("%s die%s from %s illness.",
                      you ? "You" : Monnam(mon),
                      you ? "" : "s",
                      you ? "your" : mhis(mon));
                effect = TRUE;
            }
            if (you)
                done(POISONING, delayed_killer(POISONING));
            else if (mon->usicked)
                killed(mon);
            else
                monkilled(mon, "", AD_DISE);
            mon->usicked = 0; /* in case monster lifesaved */
            break;
        }

        if (you) {
            pline(redundant ? "You feel much worse." :
                  lost ? "What a relief!" :
                  "You feel deathly sick.");
            effect = TRUE;
        } else if (vis) {
            pline(redundant ? "%s looks much worse." :
                  lost ? "%s looks relieved." :
                  "%s looks deathly sick.",
                  Monnam(mon));
            effect = TRUE;
        }
        if (lost && !redundant && you) {
            set_delayed_killer(POISONING, NULL);
            u.usick_type = 0;
        }

        if (!you && !redundant)
            mon->usicked = lost || !flags.mon_moving ? 0 : 1;
        break;
    case BLINDED:
        if (slot == os_tool) {
            if (you) {
                if (lost && blocked) {
                    pline("You can see!");
                    effect = TRUE;
                } else if (!lost && !redundant) {
                    pline("You can't see any more.");
                    effect = TRUE;
                } else if (lost) {
                    pline(redundant ? "You still can't see..." :
                          "You can see again.");
                    effect = TRUE;
                }
            }
        } else if (you || vis) {
            if (blocked) {
                if (you) {
                    pline("Your vision seems to %s for a moment but is %s now",
                          lost ? "brighten" : "dim",
                          hallu ?
                          (lost ? "sadder" : "happier") :
                          "normal");
                    effect = TRUE;
                } else if (!lost) {
                    pline("%s is briefly blinded.", Monnam(mon));
                    effect = TRUE;
                }
            } else if (redundant) {
                if (you && (slot != os_dectimeout ||
                            lost)) {
                    eyepline(lost ? "twitches" : "itches",
                             lost ? "twitch" : "itch");
                    effect = TRUE;
                }
            } else {
                if (you)
                    pline(lost && hallu ? "Far out!  A light show!" :
                          lost ? "You can see again." :
                          hallu ? "Oh, bummer!  Everything is dark! Help!" :
                          "A cloud of darkness falls upon you.");
                else
                    pline("%s %s.", Monnam(mon),
                          lost ? "can see again" : "is blinded");
                effect = TRUE;
            }
        }
        turnstate.vision_full_recalc = TRUE;
        see_monsters(FALSE);
        break;
    case SLEEPING: /* actually restful sleep */
        /* Kill the timer if the property was fully lost */
        if (lost && !redundant)
            mon->mintrinsic[prop] &= ~TIMEOUT;

        if (lost && slot == os_dectimeout) {
            int sleeptime = 0;
            if (!resists_sleep(mon) &&
                ((you && !u_helpless(hm_unconscious)) ||
                 (!you && mon->mcanmove)))
                sleeptime = rnd(20);
            if (sleeptime) {
                if (you || vis) {
                    pline("%s fall%s asleep.", you ? "You" : Monnam(mon),
                          you ? "" : "s");
                    effect = TRUE;
                }
                sleep_monst(mon, sleeptime, 0);
            }

            if (restful_sleep(mon))
                set_property(mon, prop, sleeptime + rnd(100), TRUE);
        }
        break;
    case LWOUNDED_LEGS:
        if (lost && !redundant)
            heal_legs(mon, LEFT_SIDE);
        break;
    case RWOUNDED_LEGS:
        if (lost && !redundant)
            heal_legs(mon, RIGHT_SIDE);
        break;
    case STONED:
        if (lost && slot != os_dectimeout) {
            /* Check for golem change first */
            if (blocked && poly_when_stoned(mon->data)) {
                if (you)
                    polymon(PM_STONE_GOLEM, TRUE);
                else
                    newcham(mon, &mons[PM_STONE_GOLEM], FALSE, FALSE);
                set_property(mon, prop, -2, TRUE);
                if (!you)
                    mon->ustoned = 0;
                else
                    set_delayed_killer(STONING, NULL);
            }
            if (you || vis) {
                if (hallu)
                    pline("What a pity - %s just ruined a piece of %sart!",
                          you ? "you" : mon_nam(mon),
                          ((you && ACURR(A_CHA) > 15) ||
                           mon->data == &mons[PM_SUCCUBUS] || /* Foocubi has 18 cha */
                           mon->data == &mons[PM_INCUBUS]) ? "fine " : "");
                else
                    pline("%s %s more limber!",
                          you ? "You" : Monnam(mon),
                          you ? "feel" : "looks");
                if (!you)
                    mon->ustoned = 0;
                else
                    set_delayed_killer(STONING, NULL);
                effect = TRUE;
            }
        } else {
            if (you || vis) {
                if (timer == 4)
                    pline("%s %s slowing down.",
                          you ? "You" : Monnam(mon),
                          you ? "are" : "is");
                else if (timer == 3)
                    pline("%s limbs are stiffening.",
                          you ? "Your" : s_suffix(Monnam(mon)));
                else if (timer == 2)
                    pline("%s limbs have turned to stone.",
                          you ? "Your" : s_suffix(Monnam(mon)));
                else if (timer == 1)
                    pline("%s %s turned to stone.",
                          you ? "You" : Monnam(mon),
                          you ? "have" : "has");
                else if (timer == 0)
                    pline("%s %s a statue.",
                          you ? "You" : Monnam(mon),
                          you ? "are" : "is");
                effect = TRUE;
            }
            /* remove intrinsic speed, even if mon re-acquired it */
            set_property(mon, FAST, -1, TRUE);
            if (you)
                exercise(A_DEX, FALSE);

            if (lost) { /* petrified */
                if (you)
                    done(STONING, delayed_killer(STONING));
                else
                    monstone(mon);
            } else if (timer == 5) {
                if (!you && !flags.mon_moving)
                    mon->ustoned = 1;
            } else if (timer <= 2) {
                if (you)
                    helpless(3, hr_paralyzed, "unable to move due to turning to stone",
                             NULL);
                else {
                    mon->mcanmove = 0;
                    mon->mfrozen = timer + 1;
                }
            }
        }
        break;
    case STRANGLED:
        if (lost && slot != os_dectimeout) {
            if (you || vis) { /* TODO: give a suitable message if unbreathing */
                pline("%s can breathe more easily!",
                      you ? "You" : Monnam(mon));
                effect = TRUE;
            }
            /* unset the timer, in case the loss was from removing the amulet */
            set_property(mon, prop, -2, TRUE);
            break;
        }

        if (!lost && !redundant && slot != os_dectimeout) {
            if (you || vis)
                pline("It constricts %s throat!",
                      you ? "your" : s_suffix(mon_nam(mon)));
            set_property(&youmonst, STRANGLED, 5, TRUE);
        }

        if (you)
            exercise(A_STR, FALSE);

        if (lost) {
            if (you || vis)
                pline("%s suffocate%s.",
                      you ? "You" : Monnam(mon),
                      you ? "" : "s");
            effect = TRUE;
            if (you)
                done(SUFFOCATION, killer_msg(SUFFOCATION,
                                             u.uburied ? "suffocation" : "strangulation"));
            else
                mondied(mon);
            break;
        }

        if (you || vis) {
            if (unbreathing(mon) || !rn2(50)) {
                if (timer == 4)
                    pline("%s %s is becoming constricted.",
                          you ? "Your" : s_suffix(Monnam(mon)),
                          mbodypart(mon, NECK));
                else if (timer == 3)
                    pline("%s blood is having trouble reaching %s brain.",
                          you ? "Your" : s_suffix(Monnam(mon)),
                          you ? "your" : s_suffix(mon_nam(mon)));
                else if (timer == 2)
                    pline("The pressure on %s %s increases.",
                          you ? "your" : s_suffix(mon_nam(mon)),
                          mbodypart(mon, NECK));
                else if (timer == 1)
                    pline("%s consciousness is fading.",
                          you ? "Your" : s_suffix(Monnam(mon)));
            } else {
                if (timer == 4)
                    pline("%s find%s it hard to breathe.",
                          you ? "You" : Monnam(mon), you ? "" : "s");
                else if (timer == 3)
                    pline("%s %s gasping for air.",
                          you ? "You" : Monnam(mon), you ? "are" : "is");
                else if (timer == 2)
                    pline("%s can no longer breathe.",
                          you ? "You" : Monnam(mon));
                else if (timer == 1)
                    pline("%s %s turning %s.", you ? "You" : Monnam(mon),
                          you ? "are" : "is", hcolor("blue"));
            }
            effect = TRUE;
        }
        break;
    case HALLUC:
        if (you) {
            if (lost && blocked)
                pline("Your vision seems to %s for a moment but is %s now",
                      "flatten", "normal");
            else if (lost && blind(mon))
                eyepline("itches", "itch");
            else if (lost)
                pline("Everything looks SO boring now.");
            else if (!redundant)
                pline("Oh wow!  Everything %s so cosmic!",
                      blind(mon) ? "feels" : "looks");
            effect = TRUE;
            see_monsters(TRUE);
            see_objects(TRUE);
            see_traps(TRUE);
        } else
            impossible("Monster got hallucination?");
        break;
    case HALLUC_RES:
        if (you) {
            see_monsters(TRUE);
            see_objects(TRUE);
            see_traps(TRUE);
        }
        break;
    case FUMBLING:
        /* If we gained the property and it wasn't due to a timer,
           set fumbling timeout to rnd(20) */
        if (!lost && !redundant && real_slot != os_timeout) {
            set_property(mon, prop, rnd(20), TRUE);
            break;
        }

        /* Kill the timer if the property was fully lost
           (This is redundant, but fine, if slot was os_dectimeout) */
        if (lost && !redundant)
            mon->mintrinsic[prop] &= ~TIMEOUT;

        if (lost && slot == os_dectimeout) {
            /* canmove+not eating is close enough to umoved */
            if (((you && u.umoved) ||
                 (!you && mon->mcanmove &&
                  !mon->meating)) &&
                !levitates(mon)) {
                effect = slip_or_trip(mon);
                if (you)
                    helpless(2, hr_moving, "fumbling", "");
                else {
                    mon->mcanmove = 0;
                    mon->mfrozen = 2;
                }
            }

            /* os_circumstance is ice, don't restart fumble timer */
            if (fumbling(mon) & ~W_MASK(os_circumstance))
                set_property(mon, FUMBLING, rnd(20), FALSE);
        }
        break;
    case JUMPING:
        if (you && slot == os_armf) {
            pline("Your %s feel %s.", makeplural(body_part(LEG)),
                  lost ? "shorter" : "longer");
            effect = TRUE;
        }
        break;
    case WWALKING:
        if (mon == &youmonst)
            spoteffects(TRUE);
        /* monsters are handled at end-of-turn elsewhere */
        break;
    case HUNGER:
        break;
    case GLIB:
        if (blocked) /* no message, just remove timers */
            set_property(mon, prop, -2, TRUE);
        else if (you && lost && !redundant) {
            pline("Your %s feels less slippery",
                  makeplural(body_part(FINGER)));
            effect = TRUE;
        }
        break;
    case REFLECTING:
    case LIFESAVED:
    case ANTIMAGIC:
        break;
    case DISPLACED:
        if (you && !redundant) {
            pline(lost ? "You stop shimmering" :
                  "Your outline shimmers and shifts");
            effect = TRUE;
        }
        break;
    case CLAIRVOYANT:
        if (slot == os_dectimeout && !(timer % 15)) {
            if (you && !blocked) {
                do_vicinity_map();
                effect = TRUE;
            }
        }

        /* If we still have clairvoyance, increase the timer
           if timer is 0. */
        if (!(timer % 15)) {
            if (m_has_property(mon, prop, ANY_PROPERTY, TRUE)) {
                set_property(mon, prop, max(timer, 15), TRUE);
            }
        }
        break;
    case VOMITING:
        if (lost && slot != os_dectimeout) {
            if (you || vis) {
                pline("%s %s much less nauseated now.",
                      you ? "You" : Monnam(mon),
                      you ? "feel" : "looks");
                effect = TRUE;
            }
        } else if (slot == os_dectimeout) {
            if (you || vis) {
                if (timer == 14)
                    pline("%s %s mildly nauseated.", you ? "You" : Monnam(mon),
                          you ? "are feeling" : "looks");
                if (timer == 11)
                    pline("%s %s slightly confused.", you ? "You" : Monnam(mon),
                          you ? "feel" : "looks");
                if (timer == 8)
                    pline("%s can't seem to think straight.",
                          you ? "You" : Monnam(mon));
                if (timer == 5)
                    pline("%s %s incredibly sick.", you ? "You" : Monnam(mon),
                          you ? "feel" : "looks");
                if (timer == 2)
                    pline("%s suddenly vomit%s!", you ? "You" : Monnam(mon),
                          you ? "" : "s");
                if ((timer % 3) == 2)
                    effect = TRUE;
            }

            if (you)
                exercise(A_CON, FALSE);

            if (timer == 11)
                inc_timeout(mon, CONFUSION, dice(2, 4), TRUE);
            if (timer == 8)
                inc_timeout(mon, STUNNED, dice(2, 4), TRUE);
            if (timer == 2)
                vomit(mon);
        }
        break;
    case ENERGY_REGENERATION:
    case MAGICAL_BREATHING:
    case HALF_SPDAM:
    case HALF_PHDAM:
        break;
    case SICK_RES:
        set_property(mon, SICK, -2, FALSE);
        break;
    case DRAIN_RES:
        break;
    case WARN_UNDEAD:
        if (you)
            see_monsters(FALSE);
        break;
    case CANCELLED:
    case FREE_ACTION:
        break;
    case SWIMMING:
        if (mon == &youmonst)
            spoteffects(TRUE);
        break;
    case SLIMED:
        if (lost && slot != os_dectimeout) {
            if (blocked) { /* lost by poly */
                if (flaming(mon->data))
                    burn_away_slime(mon);
            } else if (you || vis) {
                pline("The slime on %s disappears!",
                      you ? "you" : mon_nam(mon));
                effect = TRUE;
            }
            /* remove stray timers */
            set_property(mon, SLIMED, -2, TRUE);
            if (!you)
                mon->uslimed = 0;
            else
                set_delayed_killer(TURNED_SLIME, NULL);
        } else {
            /* if slimes are genocided at any point during this process, immediately remove sliming */
            if (mvitals[PM_GREEN_SLIME].mvflags & G_GENOD)
                return set_property(mon, SLIMED, -2, FALSE);

            int idx = rndmonidx();
            const char *turninto = (hallu ? (monnam_is_pname(idx)
                                             ? monnam_for_index(idx)
                                             : (idx < SPECIAL_PM &&
                                                (mons[idx].geno & G_UNIQ))
                                             ? the(monnam_for_index(idx))
                                             : an(monnam_for_index(idx)))
                                    : "a green slime");

            if (you)
                exercise(A_DEX, FALSE);

            if (you || vis) {
                if (timer == 9)
                    pline("%s %s %s very well.",
                          you ? "You" : Monnam(mon),
                          you ? "don't" : "doesn't",
                          you ? "feel" : "look");
                else if (timer == 8)
                    pline("%s %s turning a %s %s.",
                          you ? "You" : Monnam(mon),
                          you ? "are" : "is",
                          is_green(mon) && !hallu ? "more vivid shade of" : "little",
                          hcolor("green"));
                else if (timer == 6)
                    pline("%s limbs are getting oozy.",
                          you ? "Your" : s_suffix(Monnam(mon)));
                else if (timer == 4)
                    pline("%s skin begins to peel away.",
                          you ? "Your" : s_suffix(Monnam(mon)));
                else if (timer == 2)
                    pline("%s %s turning into %s.",
                          you ? "You" : Monnam(mon),
                          you ? "are" : "is",
                          turninto);
                else if (timer == 0)
                    pline("%s %s become %s.",
                          you ? "You" : Monnam(mon),
                          you ? "have" : "has",
                          turninto);
                effect = TRUE;
            }

            /* remove intrinsic speed at "oozy", even if mon re-acquired it */
            if (timer <= 6)
                set_property(mon, FAST, -1, TRUE);

            if (timer == 10) {
                if (!you && !flags.mon_moving)
                    mon->uslimed = 1;
            } else if (!timer) {
                if (you)
                    done(TURNED_SLIME, delayed_killer(TURNED_SLIME));
                else {
                    newcham(mon, &mons[PM_GREEN_SLIME], FALSE, FALSE);
                    set_property(mon, SLIMED, -2, TRUE);
                }
            }
        }
        break;
    case FIXED_ABIL:
        break;
    case FLYING:
        if (mon == &youmonst)
            spoteffects(TRUE);
        break;
    case UNCHANGING:
        set_property(mon, SLIMED, -2, TRUE);
        break;
    case PASSES_WALLS:
        if (!redundant) {
            if (you && u.utraptype == TT_PIT) {
                u.utraptype = 0;
                u.utrap = 0;
                turnstate.vision_full_recalc = TRUE;
            }
        }
        break;
    case SLOW_DIGESTION:
    case INFRAVISION:
        break;
    case WARN_OF_MON:
        if (you)
            see_monsters(FALSE);
        break;
    case XRAY_VISION:
        if (you) {
            turnstate.vision_full_recalc = TRUE;
            see_monsters(FALSE);
        }
        break;
    case DETECT_MONSTERS:
        if (you && !redundant) {
            see_monsters(FALSE);
            /* did we just get the property */
            if (!lost) {
                int x, y;
                int found_monsters = 0;
                for (x = 0; x < COLNO; x++) {
                    for (y = 0; y < ROWNO; y++) {
                        if (level->locations[x][y].mem_invis) {
                            /* don't clear object memory from below monsters */
                            level->locations[x][y].mem_invis = FALSE;
                            newsym(x, y);
                        }
                        if (MON_AT(level, x, y))
                            found_monsters++;
                    }
                }
                if (!found_monsters)
                    pline("You feel lonely");
                effect = TRUE; /* either lonely or detected stuff */
            }
        }
        break;
    case SLOW:
        if (you && !redundant) {
            pline(lost ? "Your speed returns." :
                  "You feel abnormally slow.");
            effect = TRUE;
        } else if (vis && !redundant) {
            pline(lost ? "%s speeds up." :
                  "%s slows down abnormally.",
                  Monnam(mon));
            effect = TRUE;
        }
        break;
    default:
        impossible("Unknown property: %u", prop);
        break;
    }
    return effect;
}

static boolean
is_green(struct monst *mon)
{
    if (mon->data == &mons[PM_GREMLIN] || mon->data == &mons[PM_LEPRECHAUN] ||
        /* Are wood nymphs green? */
        mon->data == &mons[PM_BABY_GREEN_DRAGON] ||
        mon->data == &mons[PM_GREEN_DRAGON] ||
        /* Are NetHack's lichens green?  Some real lichens are, some not.
         * What about guardian nagas and their hatchlings?  Their default
         * representation is green, but that's also true of hobbits, among
         * other things... */
        mon->data == &mons[PM_GARTER_SNAKE] || /* usually green and black */
        mon->data == &mons[PM_GREEN_MOLD] ||
        /* Green elves are not green; etymology is they live in forests. */
        mon->data == &mons[PM_GECKO] || mon->data == &mons[PM_IGUANA] ||
        /* Lizards come in a variety of colors.  What about crocodiles?
         * I grew up thinking crocodiles and aligators are green, but
         * Google Images seems to suggest they're more often gray/brown. */
        /* The ones below this comment are currently not relevant, since
         * you can't be slimed while polymorphed into them. */
        mon->data == &mons[PM_MEDUSA] || mon->data == &mons[PM_JUIBLEX] ||
        mon->data == &mons[PM_NEFERET_THE_GREEN] ||
        mon->data == &mons[PM_GREEN_SLIME]) {
        return TRUE;
    }
    return FALSE;
}

/* give a fumble message */
static boolean
slip_or_trip(struct monst *mon)
{
    boolean you = (mon == &youmonst);
    boolean vis = canseemon(mon);
    struct obj *otmp = vobj_at(m_mx(mon), m_my(mon));
    const char *what, *pronoun;
    boolean on_foot = TRUE;
    int pctload = 0;
    if (!you)
        pctload = (curr_mon_load(mon) * 100) / max_mon_load(mon);
    else
        pctload = (inv_weight() * 100) / weight_cap();

    if (!you && !vis) {
        if (pctload > 50 && canhear())
            pline("You hear fumbling %s.",
                  dist2(u.ux, u.uy, mon->mx, mon->my) > BOLT_LIM * BOLT_LIM ?
                  "in the distance" : "nearby");
        return FALSE; /* can't see the target anyway */
    }

    if ((you && u.usteed) || flying(mon) || levitates(mon))
        on_foot = FALSE;

    if (otmp && on_foot &&
        ((you && !u.uinwater) || (!you && waterwalks(mon))) &&
        is_pool(level, m_mx(mon), m_my(mon)))
        otmp = 0;

    if (otmp && on_foot) {      /* trip over something in particular */
        /* If there is only one item, it will have just been named during the
           move, so refer to by via pronoun; otherwise, if the top item has
           been or can be seen, refer to it by name; if not, look for rocks to
           trip over; trip over anonymous "something" if there aren't any
           rocks. */
        pronoun = otmp->quan == 1L ? "it" : Hallucination ? "they" : "them";
        what = !otmp->nexthere ? pronoun :
            (otmp->dknown || !Blind) ? doname(otmp) :
            ((otmp = sobj_at(ROCK, level, m_mx(mon), m_my(mon))) == 0 ?
             "something" : otmp-> quan == 1L ? "a rock" : "some rocks");
        if (Hallucination)
            pline("Egads!  %s bite%s %s %s!", msgupcasefirst(what),
                  (!otmp || otmp->quan == 1L) ? "s" : "",
                  you ? "your" : s_suffix(mon_nam(mon)),
                  body_part(FOOT));
        else
            pline("%s trip%s over %s.", you ? "You" : Monnam(mon),
                  you ? "" : "s", what);
    } else if (rn2(3) && is_ice(level, m_mx(mon), m_my(mon)) &&
               !levitates(mon) && !flying(mon))
        pline("%s %s%s on the ice.",
              !you ? Monnam(mon) :
              u.usteed ? Monnam(u.usteed) :
              "You", rn2(2) ? "slip" : "slide",
              you && !u.usteed ? "" : "s");
    else {
        if (on_foot) {
            switch (rn2(4)) {
            case 1:
                pline("%s trip%s over your own %s.", you ? "You" : Monnam(mon),
                      you ? "" : "s",
                      Hallucination ? "elbow" : makeplural(body_part(FOOT)));
                break;
            case 2:
                pline("%s slip%s %s.", you ? "You" : Monnam(mon),
                      you ? "" : "s",
                      Hallucination ? "on a banana peel" :
                      you ? "and nearly fall" :
                      "and nearly falls");
                break;
            case 3:
                pline("%s flounder%s.", you ? "You" : Monnam(mon), you ? "" : "s");
                break;
            default:
                pline("%s stumble%s.", you ? "You" : Monnam(mon), you ? "" : "s");
                break;
            }
        } else if (you && u.usteed) {
            switch (rn2(4)) {
            case 1:
                pline("Your %s slip out of the stirrups.",
                      makeplural(body_part(FOOT)));
                break;
            case 2:
                pline("You let go of the reins.");
                break;
            case 3:
                pline("You bang into the saddle-horn.");
                break;
            default:
                pline("You slide to one side of the saddle.");
                break;
            }
            dismount_steed(DISMOUNT_FELL);
        } else {
            /* tripping in the air */
            switch (rn2(3)) {
            case 1:
                pline("%s tumble%s in place.", you ? "You" : Monnam(mon),
                      you ? "" : "s");
                break;
            case 2:
                pline("%s lose%s %s balance!", you ? "You" : Monnam(mon),
                      you ? "" : "s", you ? "your" : mhis(mon));
                break;
            default:
                pline("%s %s a hard time controlling %s movement.",
                      you ? "You" : Monnam(mon), you ? "have" : "has",
                      you ? "your" : mhis(mon));
                break;
            }
        }
    }
    if (pctload > 50)
        pline("%s make%s a lot of noise!",
              you ? "You" : Monnam(mon), you ? "" : "s");
    return TRUE;
}

unsigned
u_have_property(enum youprop property, unsigned reasons,
                boolean even_if_blocked)
{
    return m_has_property(&youmonst, property, reasons, even_if_blocked);
}


/* Player and monster helplessness. This is currently separate from properties,
   because for player helplessness, we record a reason to place in the death
   messages. */
boolean
u_helpless(enum helpless_mask mask)
{
    int i;

    /* A lack of a cause canonically indicates that we weren't actually helpless
       for this reason. We may not have an endmsg, and the timer may already
       have expired but the helplessness not yet been canceled, so we can't use
       these as indications. */
    for (i = hr_first; i <= hr_last; ++i)
        if ((mask & (1 << i)) && *turnstate.helpless_causes[i])
            return TRUE;

    return FALSE;
}

boolean
m_helpless(const struct monst *mon, enum helpless_mask mask)
{
    if (mon == &youmonst)
        return u_helpless(mask);
    if (mon->msleeping && (mask & (1 << hr_asleep)))
        return TRUE;
    if (!mon->mcanmove && (mask & (1 << hr_paralyzed)))
        return TRUE;
    if (mon->meating && (mask & (1 << hr_busy)))
        return TRUE;
    if ((mon->m_ap_type == M_AP_OBJECT || mon->m_ap_type == M_AP_FURNITURE) &&
        (mask & (1 << hr_mimicking)))
        return TRUE;

    return FALSE;
}

/* Hack: check if a monster could sense someone else at specific X/Y coords.
   This is implemented by temporary changing mx/my to the destination, call msensem,
   and then revert mx/my to its' old values */
unsigned
msensem_xy(struct monst *viewer, struct monst *viewee,
           xchar tx, xchar ty)
{
    xchar ox = viewer->mx;
    xchar oy = viewer->my;
    viewer->mx = tx;
    viewer->my = ty;
    unsigned sensed = msensem(viewer, viewee);
    viewer->mx = ox;
    viewer->my = oy;
    return sensed;
}

/* Returns the bitwise OR of all MSENSE_ values that explain how "viewer" can
   see "viewee". &youmonst is accepted as either argument. If both arguments
   are the same, this tests if/how a monster/player can detect itself. */
unsigned
msensem(const struct monst *viewer, const struct monst *viewee)
{
    unsigned sensemethod = 0;

    /* sanity checks, so the caller doesn't have to */
    if (viewer != &youmonst)
        if (!onmap(viewer) || DEADMONSTER(viewer))
            return 0;
    if (viewee != &youmonst)
        if (!onmap(viewee) || DEADMONSTER(viewee))
            return 0;
    if (!level) {
        impossible("vision calculations during level creation: %s->%s",
                   k_monnam(viewer), k_monnam(viewee));
        return 0;
    }

    /* TODO: once levels rewrite is done, this code can be simplified (and won't
       work in its present form). */
    d_level *sz = m_mz(viewer), *tz = m_mz(viewee);
    if (sz->dnum != tz->dnum || sz->dlevel != tz->dlevel)
        return 0;
    struct level *lev = level;
    if (viewer != &youmonst)
        lev = viewer->dlevel;

    int sx = m_mx(viewer), sy = m_my(viewer),
        tx = m_mx(viewee), ty = m_my(viewee),
        dx = viewee->dx, dy = viewee->dy;

    int distance = dist2(sx, sy, tx, ty);
    int distance_displaced = (displaced(viewee) ? dist2(sx, sy, dx, dy) : 500);

    /* Special case: if either endpoint is an engulfing monster, then we want
       LOE to the square specifically, ignoring players on that square (because
       the edge of an engulfing monster blocks LOE to the player). */
    char **msensem_vizarray =
        (Engulfed && (viewer == u.ustuck || viewee == u.ustuck)) ?
        NULL : viz_array;

    /* Line of effect. clear_path is like couldsee(), but doesn't require the
       player to be at either endpoint. (If the player happens to be at one of
       the endpoints, it just calls couldsee() directly.) */
    boolean loe = clear_path(sx, sy, tx, ty, msensem_vizarray);
    /* Equavilent, but for a monster's displaced image if relevant */
    boolean loe_displaced = (displaced(viewee) &&
                             clear_path(sx, sy, dx, dy, msensem_vizarray));

    /* A special case for many vision methods: water or the ground blocking
       vision. A hiding target is also included in these, because hiding is
       often a case of using an object, part of the floor, a cranny in the
       ceiling, etc., to block vision (and even when it isn't, it should block
       vision in the same cases). */
    boolean vertical_loe =
        !(m_mburied(viewer) || m_mburied(viewee) ||
          ((!!m_underwater(viewee)) ^ (!!m_underwater(viewer))) ||
          m_mundetected(viewee));

    boolean invisible = !!invisible(viewee);

    /* For normal vision, one necessary condition is that the target must be
       adjacent or on a lit square (we assume there's just enough light in the
       dungeon that even in dark areas, adjacent squares are visible to normal
       vision). We test "lit" by testing that the square is either temporarily
       lit, or permanently lit. (We can't use the normal cansee() check because
       that doesn't work for squares outside the player's LOE, and it's possible
       that neither the viewer nor the viewee is the player.)

       TODO: templit off-level. That's very hard to implement because we don't
       run lighting calculations off-level. */
    boolean target_lit = distance <= 2 || (lev == level && templit(tx, ty)) ||
        lev->locations[tx][ty].lit;
    boolean target_lit_displaced = FALSE;
    if (displaced(viewee))
        target_lit_displaced = (distance_displaced <= 2 ||
                                (lev == level && templit(dx, dy)) ||
                                lev->locations[dx][dy].lit);

    /* TODO: Maybe infravisibility should be a property? */
    boolean infravision_ok = infravision(viewer) &&
        pm_infravisible(viewee->data);

    boolean blinded = !!blind(viewer);
    boolean see_invisible = !!see_invisible(viewer);

    if (loe && vertical_loe && !blinded) {
        if (!invisible && target_lit) {
            sensemethod |= MSENSE_VISION;
            if (loe_displaced && target_lit_displaced)
                sensemethod |= MSENSE_DISPLACED;
        }
        if (!invisible && infravision_ok) {
            sensemethod |= MSENSE_INFRAVISION;
            if (loe_displaced)
                sensemethod |= MSENSE_DISPLACED;
        }
        if (invisible && (target_lit || infravision_ok) && see_invisible) {
            sensemethod |= MSENSE_SEEINVIS | MSENSEF_KNOWNINVIS;
            if (loe_displaced && (target_lit_displaced || infravision_ok))
                sensemethod |= MSENSE_DISPLACED;
        }
    }

    /* Telepathy. The viewee needs a mind; the viewer needs either to be blind,
       or for the telepathy to be extrinsic and the viewer within BOLT_LIM. */
    if (!mindless(viewee->data) && !m_helpless(viewer, hm_unconscious)) {
        unsigned telepathy_reason = telepathic(viewer);
        if ((telepathy_reason && blinded) ||
            (telepathy_reason & (W_EQUIP | W_ARTIFACT) &&
             distance <= BOLT_LIM * BOLT_LIM))
            sensemethod |= MSENSE_TELEPATHY;
    }

    /* Astral vision. Like regular vision, but has a distance check rather than
       an LOE check. It's unclear whether this pierces blindness, because the
       only item that gives astral vision also gives blindness immunity; this
       code assumes not. */
    boolean xray = astral_vision(viewer) && (!invisible || see_invisible);
    if (vertical_loe && distance <= XRAY_RANGE * XRAY_RANGE && xray &&
        (target_lit || infravision_ok)) {
        sensemethod |= MSENSE_XRAY;
        if (distance_displaced <= XRAY_RANGE * XRAY_RANGE && xray &&
            (target_lit_displaced || infravision_ok))
            sensemethod |= MSENSE_DISPLACED;
        if (invisible && see_invisible)
            sensemethod |= MSENSEF_KNOWNINVIS;
    }

    /* Ideally scent should work around corners, but not through walls. That's
       awkward to write, though, because it'd require pathfinding. */
    if (vertical_loe && loe && distance <= 5 && has_scent(viewer->data))
        sensemethod |= MSENSE_SCENT;

    /* Monster detection. All that is needed (apart from same-level, which was
       checked earlier) is the property itself. */
    if (detects_monsters(viewer))
        sensemethod |= MSENSE_MONDETECT;

    /* Warning versus monster class. (Actually implemented as monster
       /race/.) */
    if (mworn_warntype(viewer) & viewee->data->mflags2)
        sensemethod |= MSENSE_WARNOFMON;

    /* Covetous sense. Note that the player can benefit from this too, e.g. a
       player in master lich form will be able to detect the Wizard of Yendor
       holding the Book of the Dead. */
    if (covetous_sense(viewer, viewee))
        sensemethod |= MSENSE_COVETOUS;

    /* Smell of gold, approximating 3.4.3 behaviour (which was previously in
       set_apparxy in monmove.c). Xorns can sense any monster with gold in their
       inventory. */
    if (viewer->data == &mons[PM_XORN] && money_cnt(m_minvent(viewee)))
        sensemethod |= MSENSE_GOLDSMELL;

    /* Cooperative telepathy. Friendly monsters reveal themselves to each other
       with telepathy. If one has telepathy, that one's telepathy determines how
       easily they sense each other. If both has, they can be seen everywhere */
    if (!mindless(viewer->data) && !m_helpless(viewer, hm_unconscious)) {
        unsigned telepathy_reason = telepathic(viewee);
        if ((telepathy_reason && blinded) ||
            (telepathy_reason & (W_EQUIP | W_ARTIFACT) &&
             distance <= BOLT_LIM * BOLT_LIM))
            sensemethod |= MSENSE_TEAMTELEPATHY;
        if (telepathic(viewer) && telepathic(viewee))
            sensemethod |= MSENSE_TEAMTELEPATHY;
    }

    /* Aggravate monster. If a monster has the aggravate monster property,
       every monster on the level can sense it everywhere */
    if (aggravating(viewee))
        sensemethod |= MSENSE_AGGRAVATE;

    /* Warning. This partial-senses monsters that are hostile to the viewer, and
       have a level of 4 or greater, and a distance of 100 or less. */
    if (distance <= 100 && m_mlev(viewee) >= 4 &&
        warned(viewer) &&
        mm_aggression(viewee, viewer) & ALLOW_M) {
        sensemethod |= MSENSE_WARNING;
    }

    /* Deducing the existence of a long worm via seeing a segment.

       Based on the code that was formerly worm_known in worm.c, but expanded to
       handle monster viewing.

       Note: assumes that normal vision, possibly modified by astral vision and
       see invisible, is the only way to see a long worm tail. Infravision
       doesn't work (they're cold-blooded), and currently no other types of
       vision are implemented. Detection would find the head. */
    if (viewee->wormno && (!invisible || see_invisible) &&
        vertical_loe && !blinded) {
        struct wseg *curr = viewee->dlevel->wtails[viewee->wormno];

        while (curr) {
            boolean seg_dist = dist2(sx, sy, curr->wx, curr->wy);
            boolean seg_loe =
                clear_path(sx, sy, curr->wx, curr->wy, msensem_vizarray) ||
                (xray && seg_dist <= XRAY_RANGE * XRAY_RANGE);
            boolean seg_lit = seg_dist <= 2 ||
                (lev == level && templit(curr->wx, curr->wy)) ||
                lev->locations[curr->wx][curr->wy].lit;

            if (seg_loe && seg_lit)
                sensemethod |= MSENSE_WORM;

            curr = curr->nseg;
        }
    }

    /* Calculate known invisibility, because we have all the information to
       hand, and it's a complex calculation without it. We need to be able to
       see the monster's location with normal vision, but not the monster
       itself. Also don't include warning in this (because then, we can't match
       the monster to the message). */
    if (loe && vertical_loe && !blinded && sensemethod && target_lit &&
        !(sensemethod & (MSENSE_ANYVISION | MSENSE_WARNING)))
        sensemethod |= MSENSEF_KNOWNINVIS;

    /* If the target is in item form, it's not being seen properly. Any
       vision-style detection of the target is going to not see it as a
       monster. */
    if (m_helpless(viewee, 1 << hr_mimicking) &&
        (lev != level || !Protection_from_shape_changers) &&
        (sensemethod & MSENSE_ANYVISION)) {
        sensemethod &= ~MSENSE_ANYVISION;
        sensemethod |= MSENSE_ITEMMIMIC;
    }

    /* Displacement is set unconditionally where relevant for normal vision methods
       earlier. However, if the viewer sense the method through other vision methods,
       we need to disable the displacement flag since it only affects monsters who
       sense another with only normal/invis/infra/xray and where the displaced image
       isn't on the monster itself */
    if ((sensemethod & MSENSE_DISPLACED) &&
        ((sensemethod & ~(MSENSE_ANYVISION | MSENSE_DISPLACED |
                          MSENSEF_KNOWNINVIS)) ||
         (dx == m_mx(viewer) && dy == m_my(viewer)))) {
        sensemethod &= ~MSENSE_DISPLACED;
    }

    return sensemethod;
}


/* Enlightenment and conduct */
static const char
     You_[] = "You ", are[] = "are ", were[] = "were ", have[] =
    "have ", had[] = "had ", can[] = "can ", could[] = "could ";
static const char
     have_been[] = "have been ", have_never[] = "have never ", never[] =
    "never ";

#define enl_msg(menu,prefix,present,past,suffix) \
            enlght_line(menu,prefix, final ? past : present, suffix)
#define you_are(menu,attr)            enl_msg(menu,You_,are,were,attr)
#define you_have(menu,attr)           enl_msg(menu,You_,have,had,attr)
#define you_can(menu,attr)            enl_msg(menu,You_,can,could,attr)
#define you_have_been(menu,goodthing) enl_msg(menu,You_,have_been,were, \
                                              goodthing)
#define you_have_never(menu,badthing) \
            enl_msg(menu,You_,have_never,never,badthing)
#define you_have_X(menu,something) \
            enl_msg(menu,You_,have,(const char *)"", something)

static void
enlght_line(struct nh_menulist *menu, const char *start, const char *middle,
            const char *end)
{
    const char *buf;

    buf = msgprintf("%s%s%s.", start, middle, end);
    add_menutext(menu, buf);
}

/* format increased damage or chance to hit */
static const char *
enlght_combatinc(const char *inctyp, int incamt, int final)
{
    const char *modif, *bonus;

    if (final || wizard) {
        modif = msgprintf("%+d", incamt);
    } else {
        int absamt = abs(incamt);

        if (absamt <= 3)
            modif = "small";
        else if (absamt <= 6)
            modif = "moderate";
        else if (absamt <= 12)
            modif = "large";
        else
            modif = "huge";
    }
    bonus = (incamt > 0) ? "bonus" : "penalty";
    /* "bonus to hit" vs "damage bonus" */
    if (!strcmp(inctyp, "damage")) {
        const char *ctmp = inctyp;

        inctyp = bonus;
        bonus = ctmp;
    }
    return msgprintf("%s %s %s", an(modif), bonus, inctyp);
}

void
enlighten_mon(struct monst *mon, int final)
    /* final: 0 => still in progress; 1 => over, survived; 2 => dead */
{
    int ltmp;
    int n;
    const char *title;
    const char *buf;
    struct nh_menulist menu;
    
    init_menulist(&menu);
    title = final ? "Final Attributes:" : "Current Attributes:";
    
    const char *monname = (mon == &youmonst ? "You" : Monnam(mon));
    const char *is = (mon == &youmonst ? " are " : " is ");
    const char *was = (mon == &youmonst ? " were " : " was ");
    const char *has = (mon == &youmonst ? " have " : " has ");
    const char *had = " had ";
    const char *can = " can ";
    const char *could = " could ";
    const char *see = (mon == &youmonst ? " see " : " sees ");
    const char *saw = " saw";
    n = menu.icount;

#define mon_is(menu,mon,attr)         enl_msg(menu,monname,is,was,attr)
#define mon_has(menu,mon,attr)        enl_msg(menu,monname,has,had,attr)
#define mon_can(menu,mon,attr)        enl_msg(menu,monname,can,could,attr)
#define mon_sees(menu,mon,attr)       enl_msg(menu,monname,see,saw,attr)
#define mon_x(menu,mon,attr)          enl_msg(menu,monname,"","d",attr)
    
    if (mon == &youmonst && flags.elbereth_enabled &&
        u.uevent.uhand_of_elbereth) {
        static const char *const hofe_titles[3] = {
            "the Hand of Elbereth",
            "the Envoy of Balance",
            "the Glory of Arioch"
        };
        mon_is(&menu, mon, hofe_titles[u.uevent.uhand_of_elbereth - 1]);
        if (u.ualign.record >= 20)
            mon_is(&menu, mon, "piously aligned");
        else if (u.ualign.record > 13)
            mon_is(&menu, mon, "devoutly aligned");
        else if (u.ualign.record > 8)
            mon_is(&menu, mon, "fervently aligned");
        else if (u.ualign.record > 3)
            mon_is(&menu, mon, "stridently aligned");
        else if (u.ualign.record == 3)
            mon_is(&menu, mon, "aligned");
        else if (u.ualign.record > 0)
            mon_is(&menu, mon, "haltingly aligned");
        else if (u.ualign.record == 0)
            mon_is(&menu, mon, "nominally aligned");
        else if (u.ualign.record >= -3)
            mon_is(&menu, mon, "strayed");
        else if (u.ualign.record >= -8)
            mon_is(&menu, mon, "sinned");
        else
            mon_is(&menu, mon, "transgressed");
        if (wizard) {
            buf = msgprintf(" %d", u.uhunger);
            enl_msg(&menu, "Hunger level ", "is", "was", buf);

            buf = msgprintf(" %d / %ld", u.ualign.record, ALIGNLIM);
            enl_msg(&menu, "Your alignment ", "is", "was", buf);
        }
    }


        /*** Resistances to troubles ***/
    if (resists_fire(mon))
        mon_is(&menu, mon, "fire resistant");
    if (resists_cold(mon))
        mon_is(&menu, mon, "cold resistant");
    if (resists_sleep(mon))
        mon_is(&menu, mon, "sleep resistant");
    if (resists_disint(mon))
        mon_is(&menu, mon, "disintegration-resistant");
    if (resists_elec(mon))
        mon_is(&menu, mon, "shock resistant");
    if (resists_poison(mon))
        mon_is(&menu, mon, "poison resistant");
    if (resists_magm(mon))
        mon_is(&menu, mon, "magic resistant");
    if (resists_drli(mon))
        mon_is(&menu, mon, "level-drain resistant");
    if (resists_sick(mon))
        mon_is(&menu, mon, "immune to sickness");
    if (resists_acid(mon))
        mon_is(&menu, mon, "acid resistant");
    if (resists_ston(mon))
        mon_is(&menu, mon, "petrification resistant");
    if (resists_hallu(mon))
        mon_is(&menu, mon, "hallucination resistant");
    if (mon == &youmonst && u.uinvulnerable)
        mon_is(&menu, mon, "invulnerable");
    if ((mon == &youmonst && u.uedibility) ||
        (mon != &youmonst && mon->mtame))
        mon_can(&menu, mon, "recognize detrimental food");

    /*** Troubles ***/
    if (hallucinating(mon))
        mon_is(&menu, mon, "hallucinating");
    if (stunned(mon))
        mon_is(&menu, mon, "stunned");
    if (confused(mon))
        mon_is(&menu, mon, "confused");
    if (blind(mon))
        mon_is(&menu, mon, "blinded");
    if (sick(mon)) {
        if (mon == &youmonst && (u.usick_type & SICK_VOMITABLE))
            mon_is(&menu, mon, "sick from food poisoning");
        else
            mon_is(&menu, mon, "sick from illness");
    }
    if (petrifying(mon))
        mon_is(&menu, mon, "turning to stone");
    if (sliming(mon))
        mon_is(&menu, mon, "turning into slime");
    if (strangled(mon))
        mon_is(&menu, mon, (u.uburied) ? "buried" : "being strangled");
    if (cancelled(mon))
        mon_is(&menu, mon, "cancelled");
    if (slippery_fingers(mon))
        mon_has(&menu, mon, msgcat("slippery ", makeplural(body_part(FINGER))));
    if (fumbling(mon))
        mon_x(&menu, mon, "fumble");
    if (leg_hurt(mon))
        mon_has(&menu, mon, msgcat("wounded", makeplural(body_part(LEG))));;
    if (restful_sleep(mon))
        mon_has(&menu, mon, "restful sleep");
    if (hunger(mon))
        mon_has(&menu, mon, "fast metabolism");

        /*** Vision and senses ***/
    if (see_invisible(mon))
        mon_sees(&menu, mon, "invisible");
    if (telepathic(mon))
        mon_is(&menu, mon, "telepathic");
    if (warned(mon))
        mon_is(&menu, mon, "warned");
    if (warned_of_mon(mon)) {
        int warntype = mworn_warntype(mon);
        buf = msgcat("aware of the presence of ",
                    (warntype & M2_ORC) ? "orcs" :
                    (warntype & M2_DEMON) ? "demons" :
                    "something");
        mon_is(&menu, mon, buf);
    }
    if (warned_of_undead(mon))
        mon_is(&menu, mon, "warned of undead");
    if (searching(mon))
        mon_has(&menu, mon, "automatic searching");
    if (clairvoyant(mon))
        mon_is(&menu, mon, "clairvoyant");
    if (infravision(mon))
        mon_has(&menu, mon, "infravision");
    if (detects_monsters(mon))
        mon_is(&menu, mon, "sensing the presence of monsters");
    if (mon == &youmonst && u.umconf)
        mon_is(&menu, mon, "going to confuse monsters");

        /*** Appearance and behavior ***/
    if (adorned(mon)) {
        int adorn = 0;

        /* BUG: this does the wrong thing for monsters */
        if (uleft && uleft->otyp == RIN_ADORNMENT)
            adorn += uleft->spe;
        if (uright && uright->otyp == RIN_ADORNMENT)
            adorn += uright->spe;
        if (adorn < 0)
            mon_is(&menu, mon, "poorly adorned");
        else
            mon_is(&menu, mon, "adorned");
    }
    if (invisible(mon))
        mon_is(&menu, mon, "invisible");
    else if (invisible(mon) && see_invisible(mon))
        mon_is(&menu, mon, "invisible to others");
    /* ordinarily "visible" is redundant; this is a special case for the
       situation when invisibility would be an expected attribute */
    else if (m_has_property(mon, INVIS, ANY_PROPERTY, TRUE) &&
             !invisible(mon))
        mon_is(&menu, mon, "visible");
    if (displacement(mon))
        mon_is(&menu, mon, "displaced");
    if (stealthy(mon))
        mon_is(&menu, mon, "stealthy");
    if (aggravating(mon))
        mon_x(&menu, mon, "aggravate");
    if (conflicting(mon))
        mon_is(&menu, mon, "conflicting");

        /*** Transportation ***/
    if (jumps(mon))
        mon_can(&menu, mon, "jump");
    if (teleportitis(mon)) {
        if (mon == &youmonst &&
            supernatural_ability_available(SPID_RLOC))
            mon_can(&menu, mon, "teleport at will");
        else if (mon != &youmonst && (mon->m_lev == 12 ||
                (mon->data->mflags1 & M1_TPORT)))
            mon_can(&menu, mon, "teleport at will");
        else
            mon_can(&menu, mon, "teleport");
    }
    if (teleport_control(mon))
        mon_has(&menu, mon, "teleport control");
    if (levitates_at_will(mon, FALSE, FALSE))
        mon_is(&menu, mon, "levitating, at will");
    else if (levitates(mon))
        mon_is(&menu, mon, "levitating");   /* without control */
    else if (flying(mon))
        mon_can(&menu, mon, "fly");
    if (waterwalks(mon))
        mon_can(&menu, mon, "walk on water");
    if (swims(mon))
        mon_can(&menu, mon, "swim");
    if (mon->data->mflags1 & M1_AMPHIBIOUS)
        mon_can(&menu, mon, "breathe water");
    else if (unbreathing(mon))
        mon_can(&menu, mon, "survive without air");
    if (phasing(mon))
        mon_can(&menu, mon, "walk through walls");

    /* FIXME: This is printed even if you die in a riding accident. */
    if (mon == &youmonst && u.usteed)
        mon_is(&menu, mon, msgcat("riding ", y_monnam(u.usteed)));
    if (mon == &youmonst && Engulfed)
        mon_is(&menu, mon, msgcat("swallowed by ", a_monnam(u.ustuck)));
    else if (u.ustuck && (mon == u.ustuck || mon == &youmonst)) {
        if (mon == &youmonst)
            buf = msgprintf("%s %s",
                  (Upolyd && sticks(youmonst.data)) ?
                  "holding" : "held by", a_monnam(u.ustuck));
        else
            buf = msgprintf("%s %s",
                  (Upolyd && sticks(youmonst.data)) ?
                  "held by" : "holding", "you");
        mon_is(&menu, mon, buf);
    }

    /*** Physical attributes ***/
    if (mon_hitbon(mon))
        mon_has(&menu, mon, enlght_combatinc("to hit", mon_hitbon(mon), final));
    if (mon_dambon(mon))
        mon_has(&menu, mon, enlght_combatinc("damage", mon_dambon(mon), final));
    if (slow_digestion(mon))
        mon_has(&menu, mon, "slower digestion");
    if (regenerates(mon))
        mon_x(&menu, mon, "regenerate");
    if (protected(mon) || mon_protbon(mon)) {
        int prot = mon_protbon(mon);
        if (mon == &youmonst)
            prot += u.ublessed;
        prot += m_mspellprot(mon);

        if (prot < 0)
            mon_is(&menu, mon, "ineffectively protected");
        else
            mon_is(&menu, mon, "protected");
    }
    if (shapeshift_prot(mon))
        mon_is(&menu, mon, "protected from shape changers");
    if (polymorphitis(mon))
        mon_is(&menu, mon, "polymorphing");
    if (polymorph_control(mon))
        mon_has(&menu, mon, "polymorph control");
    if ((mon == &youmonst && u.ulycn >= LOW_PM) || is_were(mon->data))
        mon_is(&menu, mon, an(mons[u.ulycn].mname));
    if (mon == &youmonst && Upolyd) {
        const char *buf;
        if (u.umonnum == u.ulycn)
            buf = "in beast form";
        else
            buf = msgprintf("%spolymorphed into %s",
                            flags.polyinit_mnum == -1 ? "" : "permanently ",
                            an(youmonst.data->mname));
        if (wizard)
            buf = msgprintf("%s (%d)", buf, u.mtimedone);
        mon_is(&menu, mon, buf);
    }
    if (unchanging(mon))
        mon_can(&menu, mon, "not change form");
    if (very_fast(mon))
        mon_is(&menu, mon, "very fast");
    else if (fast(mon))
        mon_is(&menu, mon, "fast");
    if (slow(mon))
        mon_is(&menu, mon, "slow");
    if (reflecting(mon))
        mon_has(&menu, mon, "reflection");
    if (free_action(mon))
        mon_has(&menu, mon, "free action");
    if (fixed_abilities(mon))
        mon_has(&menu, mon, "fixed abilities");
    if (will_be_lifesaved(mon))
        mon_is(&menu, mon, "life saving");
    if (mon == &youmonst && u.twoweap)
        mon_is(&menu, mon, "wielding two weapons at once");

        /*** Miscellany ***/
    if (mon == &youmonst) {
        if (Luck) {
            ltmp = abs((int)Luck);
            const char *buf = msgprintf(
                "%s%slucky",
                ltmp >= 10 ? "extremely " : ltmp >= 5 ? "very " : "",
                Luck < 0 ? "un" : "");
            if (wizard)
                buf = msgprintf("%s (%d)", buf, Luck);
            you_are(&menu, buf);
        } else if (mon == &youmonst && wizard)
            enl_msg(&menu, "Your luck ", "is", "was", " zero");
        if (u.moreluck > 0)
            you_have(&menu, "extra luck");
        else if (u.moreluck < 0)
            you_have(&menu, "reduced luck");
        if (carrying(LUCKSTONE) || stone_luck(TRUE)) {
            ltmp = stone_luck(FALSE);
            if (ltmp <= 0)
                enl_msg(&menu, "Bad luck ", "does", "did", " not time out for you");
            if (ltmp >= 0)
                enl_msg(&menu, "Good luck ", "does", "did",
                        " not time out for you");
        }

        if (u.ugangr) {
            const char *buf = msgprintf(
                " %sangry with you",
                u.ugangr > 6 ? "extremely " : u.ugangr > 3 ? "very " : "");
            if (wizard)
                buf = msgprintf("%s (%d)", buf, u.ugangr);
            enl_msg(&menu, u_gname(), " is", " was", buf);
        } else if (!final) {
            /*
            * We need to suppress this when the game is over, because death
            * can change the value calculated by can_pray(), potentially
            * resulting in a false claim that you could have prayed safely.
            */
            const char *buf = msgprintf(
                "%ssafely pray", can_pray(FALSE) ? "" : "not ");
            /* can_pray sets some turnstate that needs to be reset. */
            turnstate.pray.align = A_NONE;
            turnstate.pray.type = pty_invalid;
            turnstate.pray.trouble = ptr_invalid;
            if (wizard)
                buf = msgprintf("%s (%d)", buf, u.ublesscnt);
            you_can(&menu, buf);
        }

        const char *p, *buf = "";

        if (final < 2) {       /* still in progress, or quit/escaped/ascended */
            p = "survived after being killed ";
            switch (u.umortality) {
            case 0:
                p = !final ? NULL : "survived";
                break;
            case 1:
                buf = "once";
                break;
            case 2:
                buf = "twice";
                break;
            case 3:
                buf = "thrice";
                break;
            default:
                buf = msgprintf("%d times", u.umortality);
                break;
            }
        } else {        /* game ended in character's death */
            p = "are dead";
            switch (u.umortality) {
            case 0:
                impossible("dead without dying?");
            case 1:
                break;  /* just "are dead" */
            default:
                buf = msgprintf(" (%d%s time!)", u.umortality,
                                ordin(u.umortality));
                break;
            }
        }
        if (p)
            enl_msg(&menu, You_, "have been killed ", p, buf);

    }
    if (n == menu.icount)
        mon_has(&menu, mon, "no special properties");

    display_menu(&menu, title, PICK_NONE, PLHINT_ANYWHERE,
                NULL);
    return;
}

/* TODO: replace with enlighten_mon() */
void
enlightenment(int final)
    /* final: 0 => still in progress; 1 => over, survived; 2 => dead */
{
    int ltmp;
    const char *title;
    struct nh_menulist menu;

    init_menulist(&menu);
    title = final ? "Final Attributes:" : "Current Attributes:";

    if (flags.elbereth_enabled && u.uevent.uhand_of_elbereth) {
        static const char *const hofe_titles[3] = {
            "the Hand of Elbereth",
            "the Envoy of Balance",
            "the Glory of Arioch"
        };
        you_are(&menu, hofe_titles[u.uevent.uhand_of_elbereth - 1]);
    }

    /* note: piousness 20 matches MIN_QUEST_ALIGN (quest.h) */
    if (u.ualign.record >= 20)
        you_are(&menu, "piously aligned");
    else if (u.ualign.record > 13)
        you_are(&menu, "devoutly aligned");
    else if (u.ualign.record > 8)
        you_are(&menu, "fervently aligned");
    else if (u.ualign.record > 3)
        you_are(&menu, "stridently aligned");
    else if (u.ualign.record == 3)
        you_are(&menu, "aligned");
    else if (u.ualign.record > 0)
        you_are(&menu, "haltingly aligned");
    else if (u.ualign.record == 0)
        you_are(&menu, "nominally aligned");
    else if (u.ualign.record >= -3)
        you_have(&menu, "strayed");
    else if (u.ualign.record >= -8)
        you_have(&menu, "sinned");
    else
        you_have(&menu, "transgressed");
    if (wizard) {
        const char *buf = msgprintf(" %d", u.uhunger);
        enl_msg(&menu, "Hunger level ", "is", "was", buf);

        buf = msgprintf(" %d / %ld", u.ualign.record, ALIGNLIM);
        enl_msg(&menu, "Your alignment ", "is", "was", buf);
    }

        /*** Resistances to troubles ***/
    if (Fire_resistance)
        you_are(&menu, "fire resistant");
    if (Cold_resistance)
        you_are(&menu, "cold resistant");
    if (Sleep_resistance)
        you_are(&menu, "sleep resistant");
    if (Disint_resistance)
        you_are(&menu, "disintegration-resistant");
    if (Shock_resistance)
        you_are(&menu, "shock resistant");
    if (Poison_resistance)
        you_are(&menu, "poison resistant");
    if (Drain_resistance)
        you_are(&menu, "level-drain resistant");
    if (Sick_resistance)
        you_are(&menu, "immune to sickness");
    if (Antimagic)
        you_are(&menu, "magic-protected");
    if (Acid_resistance)
        you_are(&menu, "acid resistant");
    if (Stone_resistance)
        you_are(&menu, "petrification resistant");
    if (u.uinvulnerable)
        you_are(&menu, "invulnerable");
    if (u.uedibility)
        you_can(&menu, "recognize detrimental food");

        /*** Troubles ***/
    if (Halluc_resistance)
        enl_msg(&menu, "You resist", "", "ed", " hallucinations");
    if (final) {
        if (hallucinating(&youmonst))
            you_are(&menu, "hallucinating");
        if (stunned(&youmonst))
            you_are(&menu, "stunned");
        if (confused(&youmonst))
            you_are(&menu, "confused");
        if (blind(&youmonst))
            you_are(&menu, "blinded");
        if (sick(&youmonst)) {
            if (u.usick_type & SICK_VOMITABLE)
                you_are(&menu, "sick from food poisoning");
            if (u.usick_type & SICK_NONVOMITABLE)
                you_are(&menu, "sick from illness");
        }
    }
    if (petrifying(&youmonst))
        you_are(&menu, "turning to stone");
    if (sliming(&youmonst))
        you_are(&menu, "turning into slime");
    if (strangled(&youmonst))
        you_are(&menu, (u.uburied) ? "buried" : "being strangled");
    if (cancelled(&youmonst))
        you_are(&menu, "cancelled");
    if (slippery_fingers(&youmonst))
        you_have(&menu, msgcat("slippery ", makeplural(body_part(FINGER))));
    if (fumbling(&youmonst))
        enl_msg(&menu, "You fumble", "", "d", "");
    if (leg_hurt(&youmonst))
        you_have(&menu, msgcat("wounded ", makeplural(body_part(LEG))));;
    if (restful_sleep(&youmonst))
        enl_msg(&menu, "You ", "fall", "fell", " asleep");
    if (hunger(&youmonst))
        enl_msg(&menu, "You hunger", "", "ed", " rapidly");

        /*** Vision and senses ***/
    if (see_invisible(&youmonst))
        enl_msg(&menu, You_, "see", "saw", " invisible");
    if (telepathic(&youmonst))
        you_are(&menu, "telepathic");
    if (warned(&youmonst))
        you_are(&menu, "warned");
    if (warned_of_mon(&youmonst)) {
        int warntype = worn_warntype();
        const char *buf = msgcat(
            "aware of the presence of ",
            (warntype & M2_ORC) ? "orcs" :
            (warntype & M2_DEMON) ? "demons" : "something");
        you_are(&menu, buf);
    }
    if (warned_of_undead(&youmonst))
        you_are(&menu, "warned of undead");
    if (Searching)
        you_have(&menu, "automatic searching");
    if (Clairvoyant)
        you_are(&menu, "clairvoyant");
    if (Infravision)
        you_have(&menu, "infravision");
    if (Detect_monsters)
        you_are(&menu, "sensing the presence of monsters");
    if (u.umconf)
        you_are(&menu, "going to confuse monsters");

        /*** Appearance and behavior ***/
    if (Adornment) {
        int adorn = 0;

        if (uleft && uleft->otyp == RIN_ADORNMENT)
            adorn += uleft->spe;
        if (uright && uright->otyp == RIN_ADORNMENT)
            adorn += uright->spe;
        if (adorn < 0)
            you_are(&menu, "poorly adorned");
        else
            you_are(&menu, "adorned");
    }
    if (Invisible)
        you_are(&menu, "invisible");
    else if (Invis)
        you_are(&menu, "invisible to others");
    /* ordinarily "visible" is redundant; this is a special case for the
       situation when invisibility would be an expected attribute */
    else if (u_have_property(INVIS, ANY_PROPERTY, TRUE) && BInvis)
        you_are(&menu, "visible");
    if (Displaced)
        you_are(&menu, "displaced");
    if (Stealth)
        you_are(&menu, "stealthy");
    if (Aggravate_monster)
        enl_msg(&menu, "You aggravate", "", "d", " monsters");
    if (Conflict)
        enl_msg(&menu, "You cause", "", "d", " conflict");

        /*** Transportation ***/
    if (Jumping)
        you_can(&menu, "jump");
    if (Teleportation)
        you_can(&menu, "teleport");
    if (Teleport_control)
        you_have(&menu, "teleport control");
    if (Lev_at_will)
        you_are(&menu, "levitating, at will");
    else if (Levitation)
        you_are(&menu, "levitating");   /* without control */
    else if (Flying)
        you_can(&menu, "fly");
    if (Wwalking)
        you_can(&menu, "walk on water");
    if (Swimming)
        you_can(&menu, "swim");
    if (Breathless)
        you_can(&menu, "survive without air");
    if (Passes_walls)
        you_can(&menu, "walk through walls");

    /* FIXME: This is printed even if you die in a riding accident. */
    if (u.usteed)
        you_are(&menu, msgcat("riding ", y_monnam(u.usteed)));
    if (Engulfed)
        you_are(&menu, msgcat("swallowed by ", a_monnam(u.ustuck)));
    else if (u.ustuck) {
        const char *buf = msgprintf(
            "%s %s", (Upolyd && sticks(youmonst.data)) ? "holding" : "held by",
            a_monnam(u.ustuck));
        you_are(&menu, buf);
    }

        /*** Physical attributes ***/
    if (mon_hitbon(&youmonst))
        you_have(&menu, enlght_combatinc("to hit", mon_hitbon(&youmonst), final));
    if (mon_dambon(&youmonst))
        you_have(&menu, enlght_combatinc("damage", mon_dambon(&youmonst), final));
    if (Slow_digestion)
        you_have(&menu, "slower digestion");
    if (Regeneration)
        enl_msg(&menu, "You regenerate", "", "d", "");
    if (protected(&youmonst) || mon_protbon(&youmonst)) {
        int prot = mon_protbon(&youmonst);
        prot += m_mspellprot(&youmonst);
        prot += u.ublessed;

        if (prot < 0)
            you_are(&menu, "ineffectively protected");
        else
            you_are(&menu, "protected");
    }
    if (Protection_from_shape_changers)
        you_are(&menu, "protected from shape changers");
    if (Polymorph)
        you_are(&menu, "polymorphing");
    if (Polymorph_control)
        you_have(&menu, "polymorph control");
    if (u.ulycn >= LOW_PM)
        you_are(&menu, an(mons[u.ulycn].mname));
    if (Upolyd) {
        const char *buf;
        if (u.umonnum == u.ulycn)
            buf = "in beast form";
        else
            buf = msgprintf("%spolymorphed into %s",
                            flags.polyinit_mnum == -1 ? "" : "permanently ",
                            an(youmonst.data->mname));
        if (wizard)
            buf = msgprintf("%s (%d)", buf, u.mtimedone);
        you_are(&menu, buf);
    }
    if (Unchanging)
        you_can(&menu, "not change from your current form");
    if (Fast)
        you_are(&menu, Very_fast ? "very fast" : "fast");
    if (Reflecting)
        you_have(&menu, "reflection");
    if (Free_action)
        you_have(&menu, "free action");
    if (Fixed_abil)
        you_have(&menu, "fixed abilities");
    if (Lifesaved)
        enl_msg(&menu, "Your life ", "will be", "would have been", " saved");
    if (u.twoweap)
        you_are(&menu, "wielding two weapons at once");

        /*** Miscellany ***/
    if (Luck) {
        ltmp = abs((int)Luck);
        const char *buf = msgprintf(
            "%s%slucky",
            ltmp >= 10 ? "extremely " : ltmp >= 5 ? "very " : "",
            Luck < 0 ? "un" : "");
        if (wizard)
            buf = msgprintf("%s (%d)", buf, Luck);
        you_are(&menu, buf);
    } else if (wizard)
        enl_msg(&menu, "Your luck ", "is", "was", " zero");
    if (u.moreluck > 0)
        you_have(&menu, "extra luck");
    else if (u.moreluck < 0)
        you_have(&menu, "reduced luck");
    if (carrying(LUCKSTONE) || stone_luck(TRUE)) {
        ltmp = stone_luck(FALSE);
        if (ltmp <= 0)
            enl_msg(&menu, "Bad luck ", "does", "did", " not time out for you");
        if (ltmp >= 0)
            enl_msg(&menu, "Good luck ", "does", "did",
                    " not time out for you");
    }

    if (u.ugangr) {
        const char *buf = msgprintf(
            " %sangry with you",
            u.ugangr > 6 ? "extremely " : u.ugangr > 3 ? "very " : "");
        if (wizard)
            buf = msgprintf("%s (%d)", buf, u.ugangr);
        enl_msg(&menu, u_gname(), " is", " was", buf);
    } else
        /*
         * We need to suppress this when the game is over, because death
         * can change the value calculated by can_pray(), potentially
         * resulting in a false claim that you could have prayed safely.
         */
    if (!final) {
        const char *buf = msgprintf(
            "%ssafely pray", can_pray(FALSE) ? "" : "not ");
        /* can_pray sets some turnstate that needs to be reset. */
        turnstate.pray.align = A_NONE;
        turnstate.pray.type = pty_invalid;
        turnstate.pray.trouble = ptr_invalid;
        if (wizard)
            buf = msgprintf("%s (%d)", buf, u.ublesscnt);
        you_can(&menu, buf);
    }

    {
        const char *p, *buf = "";

        if (final < 2) {       /* still in progress, or quit/escaped/ascended */
            p = "survived after being killed ";
            switch (u.umortality) {
            case 0:
                p = !final ? NULL : "survived";
                break;
            case 1:
                buf = "once";
                break;
            case 2:
                buf = "twice";
                break;
            case 3:
                buf = "thrice";
                break;
            default:
                buf = msgprintf("%d times", u.umortality);
                break;
            }
        } else {        /* game ended in character's death */
            p = "are dead";
            switch (u.umortality) {
            case 0:
                impossible("dead without dying?");
            case 1:
                break;  /* just "are dead" */
            default:
                buf = msgprintf(" (%d%s time!)", u.umortality,
                                ordin(u.umortality));
                break;
            }
        }
        if (p)
            enl_msg(&menu, You_, "have been killed ", p, buf);
    }

    display_menu(&menu, title, PICK_NONE, PLHINT_ANYWHERE,
                 NULL);
    return;
}

void
unspoilered_intrinsics(void)
{
    int n;
    struct nh_menulist menu;

    init_menulist(&menu);

    /* Intrinsic list

       This lists only intrinsics that produce messages when gained and/or lost,
       to avoid giving away information that the player might not know. */
    n = menu.icount;

#define addmenu(x,y) if (ihas_property(&youmonst, x))   \
                          add_menutext(&menu, y);

    /* Resistances */
    addmenu(FIRE_RES, "You are fire resistant.");
    addmenu(COLD_RES, "You are cold resistant.");
    addmenu(SLEEP_RES, "You are sleep resistant.");
    addmenu(DISINT_RES, "You are disintegration resistant.");
    addmenu(SHOCK_RES, "You are shock resistant.");
    addmenu(POISON_RES, "You are poison resistant.");
    addmenu(DRAIN_RES, "You are level-drain resistant.");
    addmenu(SICK_RES, "You are immune to sickness.");
    addmenu(SEE_INVIS, "You see invisible.");
    addmenu(TELEPAT, "You are telepathic.");
    addmenu(WARNING, "You are warned.");
    addmenu(SEARCHING, "You have automatic searching.");
    addmenu(INFRAVISION, "You have infravision.");

    /* Appearance, behaviour */
    if (invisible(&youmonst) & INTRINSIC) {
        if (see_invisible(&youmonst))
            add_menutext(&menu, "You are invisible to others.");
        else
            add_menutext(&menu, "You are invisible.");
    }

    addmenu(STEALTH, "You are stealthy.");
    addmenu(AGGRAVATE_MONSTER, "You aggravate monsters.");
    addmenu(CONFLICT, "You cause conflict.");
    addmenu(JUMPING, "You can jump.");
    addmenu(TELEPORT, "You can teleport.");
    addmenu(TELEPORT_CONTROL, "You have teleport control.");
    addmenu(SWIMMING, "You can swim.");
    addmenu(MAGICAL_BREATHING, "You can survive without air.");
    addmenu(PROTECTION, "You are protected.");
    addmenu(POLYMORPH, "You are polymorphing.");
    addmenu(POLYMORPH_CONTROL, "You have polymorph control.");
    addmenu(FAST, "You are fast.");

#undef addmenu

    if (n == menu.icount)
        add_menutext(&menu, "You have no intrinsic abilities.");

    display_menu(&menu, "Your Intrinsic Statistics",
                 PICK_NONE, PLHINT_ANYWHERE, NULL);
}


void
show_conduct(int final)
{
    int ngenocided;
    struct nh_menulist menu;
    const char *buf;

    /* Create the conduct window */
    init_menulist(&menu);

    if (!u.uconduct[conduct_food])
        enl_msg(&menu, You_, "have gone", "went", " without food");
    /* But beverages are okay */
    else if (!u.uconduct[conduct_vegan])
        you_have_X(&menu, "followed a strict vegan diet");
    else if (!u.uconduct[conduct_vegetarian])
        you_have_been(&menu, "vegetarian");
    if (u.uconduct_time[conduct_food] > 1800) {
        buf = msgprintf("did not eat until turn %d",
                        u.uconduct_time[conduct_food]);
        enl_msg(&menu, You_, "", "had ", buf);
    }
    if (u.uconduct_time[conduct_vegan] > 1800) {
        buf = msgprintf("followed a strict vegan diet until turn %d",
                        u.uconduct_time[conduct_vegan]);
        enl_msg(&menu, You_, "", "had ", buf);
    }
    if (u.uconduct_time[conduct_vegetarian] > 1800) {
        buf = msgprintf("followed a strict vegetarian diet until turn %d",
                        u.uconduct_time[conduct_vegetarian]);
        enl_msg(&menu, You_, "", "had ", buf);
    }

    if (!u.uconduct[conduct_gnostic])
        you_have_been(&menu, "an atheist");
    if (u.uconduct_time[conduct_gnostic] > 1800) {
        buf = msgprintf("an atheist until turn %d",
                        u.uconduct_time[conduct_gnostic]);
        enl_msg(&menu, You_, "were ", "had been ", buf);
    }

    if (!u.uconduct[conduct_weaphit])
        you_have_never(&menu, "hit with a wielded weapon");
    else {
        buf = msgprintf("used a wielded weapon %d time%s, starting on turn %d",
                        u.uconduct[conduct_weaphit],
                        plur(u.uconduct[conduct_weaphit]),
                        u.uconduct_time[conduct_weaphit]);
        you_have_X(&menu, buf);
    }
    if (!u.uconduct[conduct_killer])
        you_have_been(&menu, "a pacifist");
    if (u.uconduct_time[conduct_killer] > 1800) {
        buf = msgprintf("a pacifist until turn %d",
                        u.uconduct_time[conduct_killer]);
        enl_msg(&menu, You_, "were ", "had been ", buf);
    }

    if (!u.uconduct[conduct_illiterate])
        you_have_been(&menu, "illiterate");
    else {
        buf = msgprintf("read items or engraved %d time%s, starting on turn %d",
                        u.uconduct[conduct_illiterate],
                        plur(u.uconduct[conduct_illiterate]),
                        u.uconduct_time[conduct_illiterate]);
        you_have_X(&menu, buf);
    }

    ngenocided = num_genocides();
    if (ngenocided == 0) {
        you_have_never(&menu, "genocided any monsters");
    } else {
        buf = msgprintf("genocided %d type%s of monster%s, starting on turn %d",
                        ngenocided, plur(ngenocided), plur(ngenocided),
                        u.uconduct_time[conduct_genocide]);
        you_have_X(&menu, buf);
    }

    if (!u.uconduct[conduct_polypile])
        you_have_never(&menu, "polymorphed an object");
    else {
        buf = msgprintf("polymorphed %d item%s, starting on turn %d",
                        u.uconduct[conduct_polypile],
                        plur(u.uconduct[conduct_polypile]),
                        u.uconduct_time[conduct_polypile]);
        you_have_X(&menu, buf);
    }

    if (!u.uconduct[conduct_polyself])
        you_have_never(&menu, "changed form");
    else {
        buf = msgprintf("changed form %d time%s, starting on turn %d",
                        u.uconduct[conduct_polyself],
                        plur(u.uconduct[conduct_polyself]),
                        u.uconduct_time[conduct_polyself]);
        you_have_X(&menu, buf);
    }

    if (!u.uconduct[conduct_wish])
        you_have_X(&menu, "used no wishes");
    else {
        buf = msgprintf("used %u wish%s, starting on turn %d",
                        u.uconduct[conduct_wish],
                        (u.uconduct[conduct_wish] > 1) ? "es" : "",
                        u.uconduct_time[conduct_wish]);
        you_have_X(&menu, buf);

        if (!u.uconduct[conduct_artiwish])
            enl_msg(&menu, You_, "have not wished", "did not wish",
                    " for any artifacts");
        else {
            buf = msgprintf("wished for your your first artifact on turn %d",
                            u.uconduct_time[conduct_artiwish]);
            you_have_X(&menu, buf);
        }
    }

    if (!u.uconduct[conduct_puddingsplit])
        you_have_never(&menu, "split a pudding");
    else {
        buf = msgprintf("split %u pudding%s, starting on turn %d",
                        u.uconduct[conduct_puddingsplit],
                        plur(u.uconduct[conduct_puddingsplit]),
                        u.uconduct_time[conduct_puddingsplit]);
        you_have_X(&menu, buf);
    }

    if (!u.uconduct[conduct_elbereth])
        enl_msg(&menu, You_, "have never written", "never wrote",
                " Elbereth's name");
    else {
        buf = msgprintf(" Elbereth's name %u time%s, starting on turn %d",
                        u.uconduct[conduct_elbereth],
                        plur(u.uconduct[conduct_elbereth]),
                        u.uconduct_time[conduct_elbereth]);
        enl_msg(&menu, You_, "have written", "wrote", buf);
    }

    if (!u.uconduct[conduct_lostalign])
        enl_msg(&menu, You_, "have never violated", "never violated",
                " your personal moral code");
    else {
        buf = msgprintf(" your moral code, losing %u point%s of alignment, "
                "starting on turn %d",
                u.uconduct[conduct_lostalign],
                plur(u.uconduct[conduct_lostalign]),
                u.uconduct_time[conduct_lostalign]);
        enl_msg(&menu, You_, "have violated", "violated", buf);
    }

    /* birth options */
    if (!flags.bones_enabled)
        you_have_X(&menu, "disabled loading bones files");
    if (!flags.elbereth_enabled)        /* not the same as not /writing/ E */
        you_have_X(&menu, "abstained from Elbereth's help");
    if (flags.permahallu)
        enl_msg(&menu, You_, "are ", "were", "permanently hallucinating");
    if (flags.permablind)
        enl_msg(&menu, You_, "are ", "were ", "permanently blind");

    /* Pop up the window and wait for a key */
    display_menu(&menu, "Voluntary challenges:", PICK_NONE,
                 PLHINT_ANYWHERE, NULL);
}
