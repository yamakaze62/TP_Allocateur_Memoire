#include "mem.h"
#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

// constante définie dans gcc seulement
#ifdef __BIGGEST_ALIGNMENT__
#define ALIGNMENT __BIGGEST_ALIGNMENT__
#else
#define ALIGNMENT 16
#endif

/* La seule variable globale autorisée
 * On trouve à cette adresse la taille de la zone mémoire
 */
static void* memory_addr;
struct fb* zl;

static inline void *get_system_memory_adr() {
	return memory_addr;
}

static inline size_t get_system_memory_size() {
	return *(size_t*)memory_addr;
}


struct fb {
	size_t size;
	struct fb* next;
	//structure définissant un bloc libre
};

//initialisation de la mémoire

void mem_init(void* mem, size_t taille)
{
    memory_addr = mem;
    *(size_t*)memory_addr = taille;
	assert(mem == get_system_memory_adr());
	assert(taille == get_system_memory_size());
	/* 
	la 1ère zone mémoire. contient la taille de la zone entière 
	et le pointeur sur la prochaine zone libre
	
	*/
	zl = (struct fb*)(memory_addr+sizeof(struct fb));
	zl->size = taille - sizeof(struct fb);
	zl->next = NULL;

	mem_fit(&mem_fit_first);
}

void mem_show(void (*print)(void *, size_t, int)) {
	void *cour = get_system_memory_adr();
	void* taille_mem = cour + get_system_memory_size();
	struct fb* prochain_ZL = zl;

	void *prem = NULL;
	while (cour != prem && cour < taille_mem) {

		//taille de la zone actuelle
		size_t taille_cour = ((struct fb*)cour)->size;
		if(cour != prochain_ZL){
			print(cour, taille_cour, 0);
		}
		else{
			print(cour, taille_cour, 1);
			prochain_ZL = prochain_ZL->next;
		}
		prem = cour;
		cour += taille_cour;
	}
}

//sert juste à chosir la stratégie de placement
static mem_fit_function_t *mem_fit_fn;

void mem_fit(mem_fit_function_t *f) {
	mem_fit_fn=f;
}

void* allign(void* addr, size_t a){
	if((intptr_t)addr % a == 0){
		return addr;
	}
	else{
		return (addr + a - (intptr_t)addr % a);
	}
}

void *mem_alloc(size_t taille) {
	__attribute__((unused))

	//on détermine la taille nécessaire pour allouer le bloc 
	//soit métadonnées + taille en paramètre

	size_t lg = (intptr_t)allign((void *)sizeof(size_t), ALIGNMENT) + 		(intptr_t)allign((void *)taille, ALIGNMENT);

	//la zone à allouer de la taille demandée par l'utilisateur	
	struct fb *zone_allouer = mem_fit_fn(zl , taille);
	
	struct fb * temp = zl; //la 1ère zone libre de la chaine

	//si aucune zone libre n'est disponible à la taille demandée :
	size_t *res = NULL;
	if(zone_allouer == NULL){
		return NULL;
	}

	if(zone_allouer != zl){
		while(temp->next != zone_allouer){
			temp = temp->next;
		}
	}
	
	//on va redéfinir la nouvelle zone libre
	if(zone_allouer->size - lg >= sizeof(struct fb)){

		//création de la zone suivante libre
		struct fb *suiv = (struct fb*)((char*)zone_allouer + lg);

		//taille de la zone suivante
		suiv->size = zone_allouer->size - lg;
		suiv->next = NULL;
		
		//adresse de la zone allouée

		res = (size_t *)zone_allouer;

		//taille de la zone allouée
		*res = lg;

		//chainage des zones libres
		if(temp != zl){
			temp->next = suiv;
			suiv->next = zone_allouer->next;
		}
		else{
			zl = suiv;
			zl->next = zone_allouer->next;
		}
		
	}
	else{
		res = (size_t *)zone_allouer;
		*res = zone_allouer->size;
		if(temp != zl){
			temp->next = zone_allouer->next;
		}
		else{
			zl->next = zone_allouer->next;
		}
	}

	return (void*)(res + (intptr_t)allign((void *)sizeof(size_t), ALIGNMENT));
}


void mem_free(void* mem) {

	// On définit notre zone mémoire à liberer ainsi que sa taille

	size_t *zo = (size_t *)mem;
	size_t taille = *zo;

	// Deux booléens nous indiquant l'état des zones contiguës
	int prec_libre = 0;
	int suiv_libre = 0;

	// On définit la nouvelle zone libre
	struct fb *temp;

	// On définit nos diffférentes zones repères qui nous serviront à lier les différentes zones libres

	struct fb* prec_prec = NULL;
	struct fb* prec = NULL;
	struct fb* cour = zl;
	
	//parcourt de la liste
	if((long)cour < (long)zo){
		prec_prec = prec;
		prec = cour;
		cour = cour->next;
	}

	if(prec != NULL && (intptr_t)prec == (intptr_t)zo - prec->size){
		prec_libre = 1;
	}

	if((intptr_t)cour == (intptr_t)zo + taille){
		suiv_libre = 1;
	}

	if(prec_libre == 1){
		temp = (struct fb*)prec;
		temp->size = prec->size + taille;
		if(prec_prec!=NULL){
			prec_prec->next = temp;
		}
		else{
			zl = temp;
		}
		temp->next = cour->next;
	}

	if(suiv_libre == 1){
		temp = (struct fb*)cour;
		temp->size = taille + cour->size;
		if(prec!=NULL){
			prec->next = temp;
		}
		else{
			zl = temp;
		}
		temp->next = cour->next;
	}
}


struct fb* mem_fit_first(struct fb *list, size_t size) {
	size_t taille = size;

	while(list!=NULL && list->size < taille){
		list = list->next;
	}
	return list;
}

/* Fonction à faire dans un second temps
 * - utilisée par realloc() dans malloc_stub.c
 * - nécessaire pour remplacer l'allocateur de la libc
 * - donc nécessaire pour 'make test_ls'
 * Lire malloc_stub.c pour comprendre son utilisation
 * (ou en discuter avec l'enseignant)
 */
size_t mem_get_size(void *zone) {
	/* zone est une adresse qui a été retournée par mem_alloc() */

	/* la valeur retournée doit être la taille maximale que
	 * l'utilisateur peut utiliser dans cette zone */
	return 0;
}

/* Fonctions facultatives
 * autres stratégies d'allocation
 */
struct fb* mem_fit_best(struct fb *list, size_t size) {
	return NULL;
}

struct fb* mem_fit_worst(struct fb *list, size_t size) {
	return NULL;
}
