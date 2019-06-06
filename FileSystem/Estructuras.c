/*
 * Estructuras.c
 *
 *  Created on: 16 may. 2019
 *      Author: utnso
 */

#include "Estructuras.h"

//Todo
/*
 * eliminar carpeta
 * eliminar directorio
 * copiar archivo (para los tmpc)
 * archivo_modificar_size();	//escribir nuevo numero en el archivo.
 * archivo_agregar_bloque();    //en el array de un archivo
 * archivo_liberar_bloque();	//en el array de un archivo
 * bloque disponible (bits arrays)
 * semaforos!
 *
 *NO FUNCIONA: string_to_upper(nomb);   //ver como hacer en crear_directorio().
 * ver actualizacion RETARDO del Archivo de config. Se actualiza en cada fin de instruccion?
 * \*/

//Pendientes Dai:
//1) Ver si el g_logger global no hace problemas..
//	 Si se crea una vez sola o ahi en el loggear_FS esta bien.

int obtener_tiempo_dump_config() {
	return (int) config_FS.tiempo_dump * 1000;
}

void  eliminar_directorio(char* tabla){

}

//---------------------------METADATA---------------------------
char* obtener_path_metadata(char* tabla) {
	return string_from_format("%s%s/Metadata", RUTA_TABLAS, tabla);
}

t_config* obtener_metadata(char* tabla) {
	char* path = obtener_path_metadata(tabla);
	t_config* metadata = config_create(path);
	free(path);
	return metadata;
}

int obtener_part_metadata(char* tabla) {
	t_config* metadata = obtener_metadata(tabla);
	int particiones = config_get_int_value(metadata, "PARTITIONS");
	config_destroy(metadata);
	return particiones;
}

char* obtener_consistencia_metadata(char* tabla) {
	t_config* metadata = obtener_metadata(tabla);
	char* consistencia = config_get_string_value(metadata, "CONSISTENCY");
	config_destroy(metadata);
	return consistencia; //free(consistencia)?
}

int obtener_tiempo_compactacion_metadata(char* tabla) {
	t_config* metadata = obtener_metadata(tabla);
	int tiempo_compactacion = config_get_int_value(metadata, "COMPACTATION_TIME");
	config_destroy(metadata);
	return tiempo_compactacion;
}


//---------------------------BLOQUES---------------------------
char* formatear_registro(registro_t* registro) {
	uint16_t key = registro->key;
	char* value = registro->value;
	char* ts = mseg_a_string(registro->timestamp);
	return string_from_format("%s;%d;%s", ts, key, value);
}

int tam_registro(registro_t* registro) {
	char* registro_formateado = formatear_registro(registro);
	int tam_registro = sizeof(registro_formateado); //strlen(registro_formateado);
	free(registro_formateado);
	return tam_registro;
}

char* obtener_ruta_bloque(int nro_bloque) {
	return string_from_format("%s%d.bin", RUTA_BLOQUES, nro_bloque);
}

void escribir_registro_bloque(registro_t* registro, char* ruta_bloque, char* ruta_archivo) {
	FILE* archivo_bloque = txt_open_for_append(ruta_bloque);
	char* string_registro = formatear_registro(registro);
	if(tam_registro(registro) <= espacio_restante_bloque(ruta_archivo))
		txt_write_in_file(archivo_bloque, string_registro);
	else {
		int tam_restante = espacio_restante_bloque(ruta_archivo);
		char* primera_mitad_registro = string_substring_until(string_registro, tam_restante);
		txt_write_in_file(archivo_bloque, primera_mitad_registro);
		int nro_bloque = siguiente_bloque_disponible();
		agregar_bloque_archivo(ruta_archivo, nro_bloque);
		char* nuevo_bloque = obtener_ruta_bloque(nro_bloque);
		FILE* archivo_nuevo_bloque = txt_open_for_append(nuevo_bloque);
		char* mitad_restante_registro = string_substring_from(string_registro, tam_restante);
		txt_write_in_file(archivo_nuevo_bloque, mitad_restante_registro);
		txt_close_file(archivo_nuevo_bloque);
		free(primera_mitad_registro);
		free(nuevo_bloque);
		free(mitad_restante_registro);
	}
	txt_close_file(archivo_bloque);
	free(string_registro);
}

//---------------------------BITARRAY---------------------------
int bloques_en_bytes() {
	return ceil(Metadata_FS.blocks/8);
}

void inicializar_bitmap() {
	FILE* archivo_bitmap = fopen(RUTA_BITMAP, "w+");
	char* bitmap = string_repeat(0, bloques_en_bytes());
	fwrite(bitmap, bloques_en_bytes(), 1, archivo_bitmap);
	fclose(archivo_bitmap);
	free(bitmap);
}

t_bitarray* levantar_bitmap() { //Pasa el contenido del bitmap a un bitarray
	FILE* archivo_bitmap = fopen(RUTA_BITMAP, "r");
	char* bitmap = malloc(bloques_en_bytes() + 1);
	int resultado_read = fread(bitmap, bloques_en_bytes(), 1, archivo_bitmap);
	if(resultado_read){
		//TODO log: error al leer archivo
	}
	t_bitarray* bitarray = bitarray_create_with_mode(bitmap, bloques_en_bytes(), LSB_FIRST);
	free(bitmap);
	return bitarray;
}

void actualizar_bitmap(t_bitarray* bitarray) {
	FILE* bitmap = fopen(RUTA_BITMAP, "w");
	fwrite(bitarray->bitarray, bloques_en_bytes(), 1, bitmap);
	fclose(bitmap);
}

int cant_bloques_disp() {
	int nro_bloque = 0;
	while(nro_bloque < Metadata_FS.blocks)
		if(!bloque_esta_ocupado(nro_bloque))
			nro_bloque++;
	return nro_bloque;
}

void eliminar_bitarray(t_bitarray* bitarray){
	bitarray_destroy(bitarray);
}

int bloque_esta_ocupado(int nro_bloque) {
	t_bitarray* bitarray = levantar_bitmap();
	int test = bitarray_test_bit(bitarray, nro_bloque);
	eliminar_bitarray(bitarray);
	return test;
}

int siguiente_bloque_disponible() {
	int nro_bloque = 0;
	while(nro_bloque < Metadata_FS.blocks && bloque_esta_ocupado(nro_bloque))
		nro_bloque++;
	return nro_bloque;
}

void ocupar_bloque(int nro_bloque) {
	t_bitarray* bitarray = levantar_bitmap();
	bitarray_set_bit(bitarray, nro_bloque);
	actualizar_bitmap(bitarray);
	eliminar_bitarray(bitarray);
}

void liberar_bloque(int nro_bloque) {
	t_bitarray* bitarray = levantar_bitmap();
	bitarray_clean_bit(bitarray, nro_bloque);
	actualizar_bitmap(bitarray);
	eliminar_bitarray(bitarray);
	//limpiar_bloque(nro_bloque); //deja vacio el archivo
}

//---------------------------TMP Y BIN---------------------------
char* pasar_a_string(char** a) {
	char* algo = "HOLA";
	return algo;
}

char* agregar_bloque_bloques(char* lista_bloques, int bloque) {
	char* algo = "HOLA";
	return algo;
}

FILE* crear_tmp(char* tabla, char* nombre_tmp){
	FILE* temporal = crear_archivo(nombre_tmp, tabla, ".tmp");
	return temporal;
}

int agregar_bloque_archivo(char* ruta_archivo, int nro_bloque) {
	if(cant_bloques_disp() == 0) {
		return 0; //TODO log: no hay bloques disponibles
	}
	t_config* archivo = config_create(ruta_archivo);
	char** bloques_ant = config_get_array_value(archivo, "BLOCKS");
	char* string_bloques_ant = pasar_a_string(bloques_ant); //posiblemente hay que hacer free
	char* bloques_tot = agregar_bloque_bloques(string_bloques_ant, nro_bloque);
	config_set_value(archivo, "BLOCKS", bloques_tot);
	config_destroy(archivo);
	return 1;
}

int obtener_tam_archivo(char* ruta_archivo) {
	t_config* archivo = config_create(ruta_archivo);
	int tam_archivo = config_get_int_value(archivo, "SIZE");
	config_destroy(archivo);
	return tam_archivo;
}

void aumentar_tam_archivo(char* ruta_archivo, registro_t* registro) {
	t_config* archivo = config_create(ruta_archivo);
	int tam_viejo = config_get_int_value(archivo, "SIZE");
	int tam_nuevo = tam_viejo + tam_registro(registro);
	char* tam = string_itoa(tam_nuevo);
	config_set_value(archivo, "SIZE", tam);
	config_destroy(archivo);
	free(tam);
}

int cantidad_bloques_usados(char* ruta_archivo) {
	t_config* archivo = config_create(ruta_archivo);
	char** lista_bloques = config_get_array_value(archivo, "BLOCKS");
	char* bloques = pasar_a_string(lista_bloques); //posiblemente hay que hacer free
	config_destroy(archivo);
	return strlen(bloques);
}

int espacio_restante_bloque(char* ruta_archivo) {
	int espacio_disponible = cantidad_bloques_usados(ruta_archivo) * Metadata_FS.block_size - obtener_tam_archivo(ruta_archivo);
	return espacio_disponible;
}


//*****************************************************************
//De aca para abajo estan corregidos y funcionan sin memory leaks.
//*****************************************************************

int crear_particiones(instr_t* instr) {

	int cantidad = atoi(obtener_parametro(instr, 2)); //cantidad de particiones?

	if (cant_bloques_disp() < cantidad){
		loggear_FS_error("Error al crear las particiones. No hay suficientes bloques disponibles",instr);
		return 0;
	}

	char* tabla = obtener_nombre_tabla(instr);

	for(int num = 1; num <= cantidad; num++) {
		char* nomb_part = string_from_format("Part_%d", num);
		FILE* archivo_binario = crear_archivo(nomb_part, tabla, ".bin");
		archivo_inicializar(archivo_binario);

		free(nomb_part);
		fclose(archivo_binario);
	}

	loggear_FS("Se crearon las particiones de la tabla correctamente.");
	return 1;
}

void crear_metadata(instr_t* instr) {
	char* tabla = obtener_nombre_tabla(instr);
	FILE* archivo_metadata = crear_archivo("Metadata", tabla, "");
	metadata_inicializar(archivo_metadata, instr);
	fclose(archivo_metadata);
	char* mensaje = string_from_format("Se creó el metadata en la tabla \"%s\".", tabla);
//	char* mensaje = malloc(sizeof(char) * (40 + strlen(tabla)));
//	sprintf(mensaje, "Se creó el metadata en la tabla \"%s\".", tabla);
	loggear_FS(mensaje);
	free(mensaje);
}

void metadata_inicializar(FILE* f, instr_t* instr) {
	char* consist = obtener_parametro(instr, 1);
	char* part = obtener_parametro(instr, 2);
	char* time = obtener_parametro(instr, 3);
	fprintf(f, "%s%s%s%s%s%s%s", "CONSISTENCY=", consist, "\nPARTITIONS=", part, "\nCOMPACTATION_TIME=", time, "\n");
}

//Inicializa con size = 0, y el array de Blocks con un bloque asignado.
//Devuelve el bloque que se le asigno al inicializarlo
int archivo_inicializar(FILE* f) {
	if (cant_bloques_disp() == 0) {
		//TODO log: error, no se puede asignar bloques al archivo
	}
	int bloque_num = siguiente_bloque_disponible();
	fprintf(f, "%s%d%s%d%s", "SIZE=", 0, "\nBLOCKS=[", bloque_num, "]\n");

	//Si hay un bloque disp se valida cuando se llama a esta funcion. Solo pasa en CREATE.
	// No hace el fclose(f);
	return bloque_num;
}

FILE* crear_archivo(char* nombre, char* tabla, char* ext) {
	char* ruta = string_from_format("%s%s/%s%s", RUTA_TABLAS, tabla, nombre, ext);
//	char* ruta = malloc(sizeof(char) * (strlen(RUTA_TABLAS) + strlen(nombre) + strlen(tabla)) + 1 + 1 + strlen(ext));
//	sprintf(ruta, "%s%s%s%s%s", RUTA_TABLAS, tabla, "/", nombre, ext);
	FILE* archivo = fopen(ruta, "w+"); //Modo: lo crea vacio para lectura y escritura. Si existe borra lo anterior.

	char* mensaje = string_from_format("Se creó el archivo %s%s en la tabla \"%s\".", nombre, ext, tabla);
//	char* mensaje = malloc(sizeof(char) * (strlen(nombre) + strlen(tabla) + 50));
//	sprintf(mensaje, "Se creó el archivo %s%s en la tabla \"%s\".", nombre, ext, tabla);
	loggear_FS(mensaje);
	free(mensaje);
	free(ruta);
	return archivo;
}

void crear_directorio(char* ruta, char * nomb) {

	//string_to_upper(nomb);   //No me funciona (??) TODO ver
	char* ruta_dir = concat(ruta, nomb);
	char* mje = malloc(sizeof(char) * (60 + strlen(ruta_dir)));
	if (!mkdir(ruta_dir, S_IRWXU)) {
		sprintf(mje, "Se creó correctamente la carpeta \"%s\" en el directorio %s", nomb, ruta);
		loggear_FS(mje);
		free(ruta_dir);
		free(mje);
	} else {
		sprintf(mje, "No se creó la carpeta \"%s\". Ya existe.", nomb);
		loggear_FS(mje);
		free(ruta_dir);
		free(mje);
	}
	//Concat hace un malloc. Aca tiene que haber un free
}

void crear_bloques() {  //Los bloques van a partir del numero 0.

	int cantidad = Metadata_FS.blocks;
	char* num;
	for (int i = 0; i <= cantidad; i++) {
		num = string_itoa(i);
		crear_bloque(num);
		free(num);
	}
	loggear_FS("Se crearon los bloques del File System.");
}

void crear_bloque(char * nombre) {
	char* ruta = malloc(sizeof(char) * (strlen(RUTA_BLOQUES) + strlen(nombre) + strlen(".bin")) + 1);
	sprintf(ruta, "%s%s%s", RUTA_BLOQUES, nombre, ".bin");
	FILE* f = fopen(ruta, "w+");
	free(ruta);
	fclose(f);
}

void loggear_FS(char *valor) {
	g_logger = log_create("Lissandra.log", "File System", 1, LOG_LEVEL_INFO);

	sem_wait(&mutex_log);
	log_info(g_logger, valor);
	sem_post(&mutex_log);

	printf("--------------\n");

	log_destroy(g_logger);
}

void loggear_FS_error(char *valor, instr_t* i) {
	g_logger = log_create("Lissandra.log", "File System", 1, LOG_LEVEL_INFO);

	sem_wait(&mutex_log);
	log_error(g_logger, valor);
	sem_post(&mutex_log);

	list_replace(i->parametros, 0, valor);

	printf("--------------\n");

	log_destroy(g_logger);
	//Esto no genera memory leak, pisa el parametro bien. testeado!
}

char* obtener_por_clave(char* ruta, char* clave) {
	t_config* c = config_create(ruta);
	char* valor;
	valor = config_get_string_value(c, clave);
	config_destroy(c);
	return valor;
}

void inicializar_configuracion(void) {
	g_config = config_create("Lissandra.config");
	config_FS.punto_montaje = config_get_string_value(g_config, "PUNTO_MONTAJE");
	config_FS.puerto_escucha = config_get_string_value(g_config, "PUERTO_ESCUCHA");
	config_FS.tamanio_value = config_get_int_value(g_config, "TAMANIO_VALUE");
	config_FS.retardo = (mseg_t)config_get_int_value(g_config, "RETARDO");
	config_FS.tiempo_dump = (mseg_t)config_get_int_value(g_config, "TIEMPO_DUMP");

	loggear_FS("Se leyó el archivo de configuración");
}

void actualizar_tiempo_dump_config(mseg_t value) {

	char* val = string_itoa((int) value);

	sem_wait(&mutex_tiempo_dump_config);
	config_set_value(g_config, "TIEMPO_DUMP", val);
	sem_post(&mutex_tiempo_dump_config);

	config_save(g_config);

	char* mje = concat("Se actualizo el tiempo de dumpeo en el archivo de configuracion. Nuevo valor: ", val);
	loggear_FS(mje);
	free(mje);
	free(val);
}

void actualizar_tiempo_retardo_config(mseg_t value) {

	char* val = string_itoa( value);

	sem_wait(&mutex_tiempo_retardo_config);
	config_set_value(g_config, "TIEMPO_RETARDO", val);
	sem_post(&mutex_tiempo_retardo_config);

	config_save(g_config);

	char* mje = concat("Se actualizo el tiempo de retardo en el archivo de configuracion. Nuevo valor: ", val);
	loggear_FS(mje);
	free(mje);
	free(val);

}

void inicializar_directorios() {

	crear_directorio("mnj/Lissandra_FS/", "Tablas");
	crear_directorio("mnj/Lissandra_FS/", "Bloques"); //vacias

	loggear_FS("Directorios listos.");

	leer_metadata_FS();

}

void leer_metadata_FS() {  //esto se lee una vez.

	char* ruta = "mnj/Lissandra_FS/Metadata/Metadata.bin";
	meta_config = config_create(ruta);
	Metadata_FS.block_size = config_get_int_value(meta_config, "BLOCK_SIZE");
	Metadata_FS.blocks = config_get_int_value(meta_config, "BLOCKS");
	Metadata_FS.magic_number = config_get_string_value(meta_config, "MAGIC_NUMBER");

	char* mensaje = malloc(sizeof(char) * 200);
	sprintf(mensaje, "%s%s%d%s%d%s%s",
			"Metadata leído. Los datos son:\n",
			"BLOCK_SIZE = ",
			Metadata_FS.block_size,
			"\nBLOCKS = ",
			Metadata_FS.blocks,
			"\nMAGIC_NUMBER = ",
			Metadata_FS.magic_number);

	loggear_FS(mensaje);
	free(mensaje);
}

