static struct list_node head = {0};

static void _try_init_list(struct list_node *list)
{
    if(list->prev == NULL || list->next == NULL)
    {
        init_list_node(list);
    }
}

static void _add_client_obj(struct list_node *list, struct ser_net_obj *obj)
{
    _try_init_list(list);

    list_append(list, &obj->node);
}

static void *_tcp_send_process(void *arg)
{
    struct ser_net_obj *client_obj = (struct ser_net_obj *)arg;
    while(!client_obj->exit_flag)
    {
    }
}

int add_ser_net_obj(struct ser_net_obj *obj)
{
    _add_client_obj(&head, obj);
    pthread_create(&client_obj->tcp_send_thread, NULL, _tcp_send_process, (void *)client_obj);
    return 0;  
}

static void *_udp_send_process(void *arg)
{
    struct ser_net_obj *client_obj = (struct ser_net_obj *)arg;
    while(!client_obj->exit_flag)
    {
    }
}

int add_udp_conn(struct ser_net_obj *obj, struct sockaddr_in *client_addr)
{
    if(!client_obj->udp_state)
    {
        client_obj->udp_state = true;
        memcpy(&client_obj->udp_client_addr, client_addr, sizeof(*client_addr));
        pthread_create(&client_obj->udp_send_thread, NULL, _udp_send_process, (void *)client_obj);
    }
    return 0;
}

struct ser_net_obj *find_client_obj(struct sockaddr_in *client_addr)
{
    struct ser_net_obj *ret;
    struct list_node *pos, *n;

    list_for_each_safe(pos, n, &head)
    {
        ret = list_node_to_entry(pos, struct ser_net_obj, node);
        if(ret->tcp_client_addr.sin_addr == client_addr.sin_addr)
        {
            return ret;
        }
    }
    return NULL;
}

int remove_ser_net_obj(struct ser_net_obj *obj)
{
    struct ser_net_obj *ret;
    struct list_node *pos, *n;

    list_for_each_safe(pos, n, &head)
    {
        ret = list_node_to_entry(pos, struct ser_net_obj, node);
        if(ret == obj)
        {
            list_del(&ret->node);
        }
    }
    obj->exit_flag = true;
    
    if(client_obj->udp_state)
        pthread_join(obj->udp_send_thread);

    pthread_join(obj->tcp_send_thread);
}