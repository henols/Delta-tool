/*
 * © 2017 henols@gmail.com
 *
 * config.h
 *
 *  Created on: 6 apr. 2017
 *      Author: Henrik
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#define HEADER_MACIC "DeltaTool/patch"
//#define HEADER_MACIC "ENDSLEY/BSDIFF43"

//#define BSPATCH_EXECUTABLE
//#define BSDIFF_EXECUTABLE
#define COMPRESS
#define DECOMPRESS

#if defined (BSDIFF_EXECUTABLE)
#define COMPRESS
#elif defined (BSPATCH_EXECUTABLE)
#define DECOMPRESS
#endif



#endif /* CONFIG_H_ */
