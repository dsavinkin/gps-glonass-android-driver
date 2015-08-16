/*
 * Copyright © 2015 Denys Petrovnin <dipcore@gmail.com>
 */

#include"gps.h"

static char line[ NMEA_MAX_SIZE + 1 ];
static bool overflow;
static int pos;

void nmea_reader_append (char *buff, int size)
{
	for (int n = 0; n < size; n++){

		if (overflow) {
			overflow = (buff[n] != '\n');
			return;
		}

		if (pos >= NMEA_MAX_SIZE ) {
			overflow = true;
			pos = 0;
			return;
		}

		line[pos] = (char) buff[n];
		pos++;

		if (buff[n] == '\n') {
			line[pos] = '\0';
			nmea_reader_parse(line);
			pos = 0;
		}

	}
}

void nmea_reader_parse(char *line) {
	switch (minmea_sentence_id(line, false)) {
		case MINMEA_SENTENCE_RMC: {
			struct minmea_sentence_rmc frame;
			if (minmea_parse_rmc(&frame, line)) {
				D("$xxRMC: raw coordinates and speed: (%d/%d,%d/%d) %d/%d\n",
						frame.latitude.value, frame.latitude.scale,
						frame.longitude.value, frame.longitude.scale,
						frame.speed.value, frame.speed.scale);
				D("$xxRMC fixed-point coordinates and speed scaled to three decimal places: (%d,%d) %d\n",
						minmea_rescale(&frame.latitude, 1000),
						minmea_rescale(&frame.longitude, 1000),
						minmea_rescale(&frame.speed, 1000));
				D("$xxRMC floating point degree coordinates and speed: (%f,%f) %f\n",
						minmea_tocoord(&frame.latitude),
						minmea_tocoord(&frame.longitude),
						minmea_tofloat(&frame.speed));

				
				notifier_set_date_time(frame.date, frame.time);
				notifier_set_latlong(minmea_tocoord(&frame.latitude), minmea_tocoord(&frame.longitude));
				notifier_set_speed(minmea_tofloat(&frame.speed));
				notifier_set_bearing(minmea_tofloat(&frame.course));
			}
			else {
				D("$xxRMC sentence is not parsed\n");
			}
		} break;

		case MINMEA_SENTENCE_GGA: {
			struct minmea_sentence_gga frame;
			if (minmea_parse_gga(&frame, line)) {
				D("$xxGGA: fix quality: %d\n", frame.fix_quality);
				D("$xxGGA: latitude: %f\n", minmea_tocoord(&frame.latitude));
				D("$xxGGA: longitude: %f\n", minmea_tocoord(&frame.longitude));
				D("$xxGGA: fix quality: %d\n", frame.fix_quality);
				D("$xxGGA: satellites tracked: %d\n", frame.satellites_tracked);
				D("$xxGGA: hdop: %f\n", minmea_tofloat(&frame.hdop));
				D("$xxGGA: altitude: %f %c\n", minmea_tofloat(&frame.altitude), frame.altitude_units);
				D("$xxGGA: height: %f %c\n", minmea_tofloat(&frame.height), frame.height_units);

				notifier_set_latlong(minmea_tocoord(&frame.latitude), minmea_tocoord(&frame.longitude));
				notifier_set_altitude(minmea_tofloat(&frame.altitude), frame.altitude_units);

				// TODO figure out how to get accuracy, is it EPE ?
				// Use hdop value for now
				notifier_set_accuracy(minmea_tofloat(&frame.hdop));
			}
			else {
				D("$xxGGA sentence is not parsed\n");
			}
		} break;

		case MINMEA_SENTENCE_GSA: {
			struct minmea_sentence_gsa frame;
			char talker[3];
			if (minmea_parse_gsa(&frame, line) && minmea_talker_id(talker, line)) {
				D("$%sGSA: mode: %c\n", talker, frame.mode);
				D("$%sGSA: fix type: %d\n", talker, frame.fix_type);
				D("$%sGSA: pdop: %f\n", talker, minmea_tofloat(&frame.pdop));
				D("$%sGSA: hdop: %f\n", talker, minmea_tofloat(&frame.hdop));
				D("$%sGSA: vdop: %f\n", talker, minmea_tofloat(&frame.vdop));

				notifier_svs_used_ids(frame.sats);
				notifier_set_accuracy(minmea_tofloat(&frame.hdop));
			}
		} break;

		case MINMEA_SENTENCE_GSV: {
			struct minmea_sentence_gsv frame;
			char talker[3];
			if ( minmea_parse_gsv(&frame, line) && minmea_talker_id(talker, line) ) {

				D("$%sGSV: message %d of %d\n", talker, frame.msg_nr, frame.total_msgs);
				D("$%sGSV: sattelites in view: %d\n", talker, frame.total_sats);

				notifier_svs_update_status(talker, frame.msg_nr, frame.total_msgs);
				notifier_svs_inview(talker, frame.total_sats);

				for (int i = 0; i < 4; i++) {

					notifier_svs_append(talker, frame.sats[i].nr, frame.sats[i].elevation, frame.sats[i].azimuth, frame.sats[i].snr);

					D("$%sGSV: sat nr %d, elevation: %d, azimuth: %d, snr: %d dbm\n",
						talker,
						frame.sats[i].nr,
						frame.sats[i].elevation,
						frame.sats[i].azimuth,
						frame.sats[i].snr);
				}
			}
			else {
				D("$xxGSV sentence is not parsed\n");
			}
		} break;

		case MINMEA_SENTENCE_VTG: {
		   struct minmea_sentence_vtg frame;
		   if (minmea_parse_vtg(&frame, line)) {
				D("$xxVTG: true track degrees = %f\n",
					   minmea_tofloat(&frame.true_track_degrees));
				D("        magnetic track degrees = %f\n",
					   minmea_tofloat(&frame.magnetic_track_degrees));
				D("        speed knots = %f\n",
						minmea_tofloat(&frame.speed_knots));
				D("        speed kph = %f\n",
						minmea_tofloat(&frame.speed_kph));

				notifier_set_speed(minmea_tofloat(&frame.speed_knots));
				notifier_set_bearing(minmea_tofloat(&frame.true_track_degrees));
		   }
		   else {
				D("$xxVTG sentence is not parsed\n");
		   }
		} break;

		case MINMEA_INVALID: {
			D("$xxxxx sentence is not valid\n");
		} break;

		default: {
			D("$xxxxx sentence is not parsed\n");
		} break;
	}
	notifier_push_location();
}