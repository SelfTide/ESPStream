// we are only writing to the first client
		if(thread_args->sc.client_s[0] != 0 
			&& thread_args->start_capture
			&& stack_count > 0)
		{
			pthread_mutex_lock(&lock);
			
			ESP_LOGI(TAG, "popping stack\n");
			dpack = pop_stack();
			stack_count--;
			ESP_LOGI(TAG, "data pack length: %d\n", dpack->length);
			send(thread_args->sc.client_s[0], (const uint8_t *)dpack->data, dpack->length, 0);
			
			pthread_cond_signal(&cond);
			pthread_mutex_unlock(&lock);
		}