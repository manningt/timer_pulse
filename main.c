#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
// #include <sys/types.h>
#include <stdint.h>
#include <sys/resource.h> //setpriority
#include <sched.h> //setsched
#include <gpiod.h>

// prototypes
static void handler(int sig, siginfo_t *si, void *uc);
bool set_scheduling();

 // the following struct is written by main and used by the signal handler
struct t_eventData
{
   struct gpiod_line *strobe_pin;
   uint64_t timer_count; //used for odd/even
   uint32_t skip_frame;
   uint32_t number_of_frames_to_skip;
   uint32_t skipped_frame_count;
   uint32_t extra_frame;
};

#define UNUSED(x) (void)(x)

int main(int argc, char **argv)
{
   int option= 0;
	uint16_t pulse_period= 16682; //defaults
	uint8_t gpio_pin= 23;

   struct t_eventData eventData = {
      .timer_count= 0, 
      .skip_frame= 0, .skipped_frame_count= 2, .number_of_frames_to_skip= 0, 
      .extra_frame= 0};

	while ((option = getopt(argc, argv, "hp:g:s:e:n:")) != -1) {
      switch (option) {
         case 'h': 
				printf("tpulse options\n"
					"  -h\t\t"
					"Print this help and exit.\n");
				printf("  -p\t\t"
					"Specify period in millisecond, e.g -p16667; default is 16682\n");
				printf("  -a\t\t"
					"Specify which RPi GPIO pin to use, e.g. -g18; default is GPIO pin 23\n");
				printf("  -s\t\t"
					"Skip pulses every 's' pulses; default is 0 - no pulse skipping\n");
				printf("  -d\t\t"
					"Number of pulses to skip; default is 1 (only used when -s is specified)\n");
				printf("  -e\t\t"
					"Insert a pulse every 'e' pulses; default is 0 - no extra pulses \n");
            exit(0);
         case 'p':
            pulse_period = atoi(optarg);
				break;
         case 'g':
            gpio_pin = atoi(optarg);
				break;
         case 's':
            eventData.skip_frame= (atoi(optarg) * 2) + 1;
				break;
         case 'n':
            eventData.number_of_frames_to_skip= (atoi(optarg) * 2);
				break;
         case 'e':
            eventData.extra_frame= (atoi(optarg) * 2) + 1;
				break;
			case '?':
            printf("unknown option: %c\n", optopt);
            exit(1);
      }
   }
   set_scheduling();

   //set up GPIO pin:
   struct gpiod_chip *chip;
   chip = gpiod_chip_open_by_name("gpiochip0");
	if (!chip)
   {
		fprintf(stderr, "gpiod_chip_open failed\n");
      exit(-1);
   }
	else
	{
		eventData.strobe_pin= gpiod_chip_get_line(chip, gpio_pin);
		if (!eventData.strobe_pin)
      {
			fprintf(stderr, "gpiod_chip_get_line failed");
         exit(-1);
      }
		else
		{
			if (gpiod_line_request_output(eventData.strobe_pin, "boomer", 0) < 0) // set as output
         {
				fprintf(stderr, "gpiod_line_request_output failed");
            exit(-1);
         }
		}
	}

   timer_t timerId = 0;
   struct sigevent sev = {0};
   struct sigaction sa = {0}; //specifies the action when receiving a signal

   // specify start delay and interval - 8341000 is 16682 msec divide by 2
   // the interval is a divide by 2 in order to assert/deassert during the interval
   struct itimerspec its = {.it_value.tv_sec = 0,
                            .it_value.tv_nsec = 1000,
                            .it_interval.tv_sec = 0,
                            .it_interval.tv_nsec = (pulse_period*1000/2)};

   sev.sigev_notify = SIGEV_SIGNAL; // Linux-specific
   sev.sigev_signo = SIGRTMIN;
   sev.sigev_value.sival_ptr = &eventData;

   if (timer_create(CLOCK_REALTIME, &sev, &timerId) != 0)
   {
      fprintf(stderr, "Error timer_create: %s\n", strerror(errno));
      exit(-1);
   }

   // specify signal and handler
   sa.sa_flags = SA_SIGINFO;
   sa.sa_sigaction = handler;

   sigemptyset(&sa.sa_mask); //initialize signal
   //  printf("Establishing handler for signal %d\n", SIGRTMIN);
   if (sigaction(SIGRTMIN, &sa, NULL) == -1)
   {
      fprintf(stderr, "Error on signal handler register: sigaction: %s\n", strerror(errno));
      exit(-1);
   }

   printf("Pulsing GPIO %u with a period of %u milliseconds", gpio_pin, pulse_period);
   if (eventData.skip_frame)
      printf(" -- and skipping every %u frame for %u frames.\n", 
         eventData.skip_frame/2, eventData.number_of_frames_to_skip/2);
   else
      printf(".\n");
   
   if (timer_settime(timerId, 0, &its, NULL) != 0) //start timer
   {
      fprintf(stderr, "Error timer_settime: %s\n", strerror(errno));
      exit(-1);
   }

   printf("   Press ENTER to Exit\n");
   while (getchar() != '\n') {}
   return 0;
}

static void handler(int sig, siginfo_t *si, void *uc)
{
   UNUSED(sig);
   UNUSED(uc);
   struct t_eventData *data = (struct t_eventData *)si->_sifields._rt.si_sigval.sival_ptr;

   data->timer_count++;
   if (data->skip_frame > 0 && (data->timer_count % data->skip_frame == 0) && data->skipped_frame_count == 0)
   {
      data->skipped_frame_count= 1;
      // printf("turning on skip count on timer_count=%llu\n", data->timer_count);
   }

   if (data->skip_frame == 0 || data->skipped_frame_count == 0)
      gpiod_line_set_value(data->strobe_pin, (data->timer_count & 0x1));

   if (data->skipped_frame_count > 0 && ++data->skipped_frame_count >= data->number_of_frames_to_skip)
   {
      data->skipped_frame_count= 0;
      // printf("turning off skip count on timer_count=%llu\n", data->timer_count);
   }
}

bool set_scheduling()
{
   char err_string[80];
	int rc = setpriority(PRIO_PROCESS, 0, -20);
	if (rc)
	{
      sprintf(err_string, "Error on setpriority %s\n", strerror(errno));
		fprintf(stderr, err_string);
 		return true;
	}
	const struct sched_param priority = { 32 };
	rc = sched_setscheduler(0, SCHED_FIFO, &priority);
	if (rc)
	{
      sprintf(err_string, "Error on setscheduler %s\n", strerror(errno));
		fprintf(stderr, err_string);
 		return true;
	}
	return false;
}
